#pragma once
#include "chunk.h"
#include <unordered_map>
#include <iostream>
#include <thread>
#include <set>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <atomic>
#include <unordered_set>
#include <utility>

struct MeshJob {
    uint64_t key;
    glm::ivec3 cpos;
    Chunk* chunk;        // safe as long as you don’t delete chunks while jobs are running
};

struct MeshResult {
    uint64_t key;
    Chunk* chunk;
    MeshData mesh_data;
    // std::vector<float> vertices;
    // std::vector<unsigned int> indices;
};



constexpr int BITS = 21;
constexpr uint64_t MASK = (1ull << BITS) - 1; // 0x1FFFFF
constexpr int OFFSET = (1 << (BITS-1)); // offset to encode signed -> unsigned

class VoxelGrid : public Drawable, public Transformable {
public:
    // int chunk_render_distance = 8;
    glm::ivec3 chunk_render_size;
    glm::ivec3 chunk_size;
    std::unordered_map<uint64_t, Chunk*> chunks;
    std::set<uint64_t> chunks_to_update;
    Chunk* test_chunk;
    

    VoxelGrid(glm::ivec3 chunk_size, glm::ivec3 chunk_render_size = {16, 6, 16});
    ~VoxelGrid();

    uint64_t pack_key(int32_t cx, int32_t cy, int32_t cz) {
        static_assert(BITS > 0 && 3 * BITS <= 64);

        if (cx < -OFFSET || cx > OFFSET - 1 ||
            cy < -OFFSET || cy > OFFSET - 1 ||
            cz < -OFFSET || cz > OFFSET - 1) {
            throw std::out_of_range("chunk coord out of packable range");
        }

        auto enc = [](int32_t c) -> uint64_t {
            uint64_t u = static_cast<uint64_t>(static_cast<int64_t>(c) + OFFSET);
            return u & MASK;
        };

        uint64_t ux = enc(cx);
        uint64_t uy = enc(cy);
        uint64_t uz = enc(cz);

        return (ux << (BITS * 2)) | (uy << BITS) | uz;
    }

    void update_chunk_meshes();


    // void unpack_key(uint64_t key, int32_t &cx, int32_t &cy, int32_t &cz) {
    //     uint64_t ux = (key >> (BITS*2)) & MASK;
    //     uint64_t uy = (key >> BITS) & MASK;
    //     uint64_t uz = key & MASK;
    //     cx = (int)( (int)ux - OFFSET );
    //     cy = (int)( (int)uy - OFFSET );
    //     cz = (int)( (int)uz - OFFSET );

    //     return (glm::vec3){cx, cy, cz}
    // }

    glm::vec3 unpack_key(uint64_t key) {
        uint64_t ux = (key >> (BITS*2)) & MASK;
        uint64_t uy = (key >> BITS) & MASK;
        uint64_t uz = key & MASK;
        int32_t cx = (int)( (int)ux - OFFSET );
        int32_t cy = (int)( (int)uy - OFFSET );
        int32_t cz = (int)( (int)uz - OFFSET );

        return glm::ivec3(cx, cy, cz);
    }

    bool is_voxel_free(glm::ivec3 pos);

    std::thread mesh_updating_thread;
    std::atomic<bool> mesh_thread_running{false};

    std::mutex jobs_mx;
    std::condition_variable jobs_cv;
    std::deque<MeshJob> jobs;
    std::unordered_set<uint64_t> in_flight;

    void enqueue_mesh_job(uint64_t key, glm::ivec3 cpos, Chunk* chunk);
    void mesh_worker_loop();
    void drain_mesh_results();

    std::mutex results_mx;
    std::deque<MeshResult> results;

    void update(Camera* camera);
    void draw(RenderState state) override;
};