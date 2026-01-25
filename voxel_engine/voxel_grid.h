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
#include "../window.h"
#include "voxel_editor.h"
#include <algorithm> 

struct MeshJob {
    uint64_t key;
    glm::ivec3 cpos; // chunk coords
    glm::ivec3 chunk_size;
    uint32_t revision;
    uint32_t ticket;

    // std::shared_ptr<const std::vector<Voxel>> voxels;
    std::shared_ptr<const std::vector<Voxel>> self;
    std::array<std::shared_ptr<const std::vector<Voxel>>, 6> nb;
};

struct MeshResult {
    uint64_t key;
    MeshData mesh_data;
    uint32_t revision;
    uint32_t ticket;
    // std::vector<float> vertices;
    // std::vector<unsigned int> indices;
};

struct GenJob {
    uint64_t key;
    glm::ivec3 cpos;
    glm::ivec3 chunk_size;
};

struct GenResult {
    uint64_t key;
    glm::ivec3 cpos;
    std::shared_ptr<const std::vector<Voxel>> voxels;
};

class VoxelGrid;




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
    // bool placed = false;
    
    VoxelGrid(glm::ivec3 chunk_size, glm::ivec3 chunk_render_size = {16, 6, 16});
    ~VoxelGrid();

    static uint64_t pack_key(int32_t cx, int32_t cy, int32_t cz) {
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

    // void update_chunk_meshes();


    // void unpack_key(uint64_t key, int32_t &cx, int32_t &cy, int32_t &cz) {
    //     uint64_t ux = (key >> (BITS*2)) & MASK;
    //     uint64_t uy = (key >> BITS) & MASK;
    //     uint64_t uz = key & MASK;
    //     cx = (int)( (int)ux - OFFSET );
    //     cy = (int)( (int)uy - OFFSET );
    //     cz = (int)( (int)uz - OFFSET );

    //     return (glm::vec3){cx, cy, cz}
    // }

    static glm::vec3 unpack_key(uint64_t key) {
        uint64_t ux = (key >> (BITS*2)) & MASK;
        uint64_t uy = (key >> BITS) & MASK;
        uint64_t uz = key & MASK;
        int32_t cx = (int)( (int)ux - OFFSET );
        int32_t cy = (int)( (int)uy - OFFSET );
        int32_t cz = (int)( (int)uz - OFFSET );

        return glm::ivec3(cx, cy, cz);
    }

    bool is_voxel_free(glm::ivec3 pos);

    // std::thread mesh_updating_thread;
    std::vector<std::thread> mesh_workers;
    std::atomic<bool> mesh_thread_running{false};

    std::mutex jobs_mx;
    std::condition_variable jobs_cv;
    std::deque<MeshJob> jobs;
    std::unordered_set<uint64_t> in_flight;

    template<class F>
    void edit_voxels(F&& apply_edits) {
        VoxelEditor voxel_editor = VoxelEditor(this);
        apply_edits(voxel_editor);

        voxel_editor.update_and_schedule();
    }

    // template<class F>
    // void edit_chunk(glm::ivec3 chunk_pos, F&& apply_edits) {
    //     uint64_t key = pack_key(chunk_pos.x, chunk_pos.y, chunk_pos.z);
        
    //     Chunk* chunk = nullptr;

    //     auto it = chunks.find(key);
    //     if (it == chunks.end())
    //         chunk = it->second;
    //     else {
    //         chunk = new Chunk(chunk_size, {1, 1, 1});
    //         chunk->position = glm::vec3(chunk_pos.x * chunk_size.x, chunk_pos.y * chunk_size.y, chunk_pos.z * chunk_size.z);
    //     }
            
    //     chunk->edit_voxels([&](std::vector<Voxel>& voxels){
            
    //     });
    // }

    template<class F>
    void edit_chunk(glm::ivec3 chunk_pos, Chunk* chunk, F&& apply_edits) {
        uint64_t key = pack_key(chunk_pos.x, chunk_pos.y, chunk_pos.z);   

        chunk->edit_voxels([&](std::vector<Voxel>& voxels){
            apply_edits(voxels);
        });

        chunks[key] = chunk;
        chunks_to_update.insert(key);
        chunks_to_update.insert(pack_key(chunk_pos.x-1, chunk_pos.y, chunk_pos.z)); // left
        chunks_to_update.insert(pack_key(chunk_pos.x, chunk_pos.y, chunk_pos.z-1)); // back
        chunks_to_update.insert(pack_key(chunk_pos.x+1, chunk_pos.y, chunk_pos.z)); // right
        chunks_to_update.insert(pack_key(chunk_pos.x, chunk_pos.y, chunk_pos.z+1)); // front
        chunks_to_update.insert(pack_key(chunk_pos.x, chunk_pos.y+1, chunk_pos.z)); // top
        chunks_to_update.insert(pack_key(chunk_pos.x, chunk_pos.y-1, chunk_pos.z)); // bottom
    }

    static glm::ivec3 get_chunk_pos(glm::vec3 pos, glm::ivec3 chunk_size) {
        int cx = pos.x / chunk_size.x + ((int)pos.x % chunk_size.x < 0 ? -1 : 0);
        int cy = pos.y / chunk_size.y + ((int)pos.y % chunk_size.y < 0 ? -1 : 0);
        int cz = pos.z / chunk_size.z + ((int)pos.z % chunk_size.z < 0 ? -1 : 0);

        return glm::ivec3(cx, cy, cz);
    }

    static inline uint32_t hash32(uint32_t x) {
        x += 0x9e3779b9u;
        x = (x ^ (x >> 16)) * 0x85ebca6bu;
        x = (x ^ (x >> 13)) * 0xc2b2ae35u;
        x ^= (x >> 16);
        return x;
    }

    static inline uint32_t rand2i(int32_t x, int32_t y) {
        // Mix coords into a 32-bit seed first (works fine with negatives)
        uint32_t ux = (uint32_t)x;
        uint32_t uy = (uint32_t)y;
        uint32_t seed = ux * 0x8da6b343u ^ uy * 0xd8163841u; // big odd constants
        return hash32(seed);
    }

    Voxel get_voxel(glm::ivec3 pos);

    bool enqueue_mesh_job(uint64_t key, glm::ivec3 cpos, Chunk* chunk);
    void mesh_worker_loop();
    void drain_mesh_results();

    std::mutex results_mx;
    std::deque<MeshResult> results;

    
    std::vector<std::thread> gen_workers;
    std::atomic<bool> gen_thread_running{false};
    std::mutex gen_jobs_mx;
    std::condition_variable gen_jobs_cv;
    std::deque<GenJob> gen_jobs;
    std::mutex gen_results_mx;
    std::deque<GenResult> gen_results;

    std::shared_ptr<std::vector<Voxel>> generate_chunk(glm::ivec3 chunk_pos, glm::ivec3 chunk_size);

    void gen_worker_loop();
    void enqueue_gen_job(uint64_t key, glm::ivec3 cpos, glm::ivec3 chunk_size);
    void drain_gen_results();
    


    void update(Window* window, Camera* camera);
    void draw(RenderState state) override;
};