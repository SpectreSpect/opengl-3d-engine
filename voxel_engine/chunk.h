#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>
#include <iostream>   // NOTE: use <iostream>, not "iostream"

#include "../mesh.h"
#include "voxel.h"
#include "../gridable.h"


struct MeshData {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
};

enum class Face {Left, Right, Back, Front, Top, Bottom};

class VoxelGrid;

class Chunk : public Drawable, public Gridable, public Transformable {
public:
    glm::ivec3 size;
    glm::vec3 voxel_size;
    VertexLayout* vertex_layout = nullptr;
    std::atomic<uint32_t> revision{0};
    // std::atomic<uint32_t> mesh_ticket{0};
    std::shared_ptr<const std::vector<Voxel>> voxels;
    bool empty_mesh = false;

    Mesh* mesh = nullptr;
    Chunk(glm::ivec3 size, glm::vec3 voxel_size);
    ~Chunk();

    template<class F>
    void edit_voxels(F&& apply_edits) {
        auto cur = std::atomic_load(&voxels);
        auto next = std::make_shared<std::vector<Voxel>>(*cur);

        apply_edits(*next);

        std::atomic_store(&voxels, std::shared_ptr<const std::vector<Voxel>>(next));
        revision.fetch_add(1, std::memory_order_relaxed);
    }

    virtual void set_voxels(std::vector<Voxel>& voxels, std::vector<glm::ivec3> positions);
    virtual void set_voxel(Voxel& voxel, glm::ivec3 position);
    virtual const Voxel* get_voxel(Voxel& voxel, glm::ivec3 position);

    void upload_mesh_gpu(MeshData& mesh_data);
    static MeshData build(const std::vector<Voxel>& voxels, glm::ivec3 size);
    static MeshData build(const std::vector<Voxel>& self, 
                          const std::array<std::shared_ptr<const std::vector<Voxel>>,6>& nb, 
                          glm::ivec3 csize);
    static void push_vertex(std::vector<float>& v, const glm::vec3& pos, const glm::vec3& normal, const glm::vec3& color);
    static void emit_face(MeshData& out, glm::ivec3 pos, Face f, glm::vec3 color);
    static bool in_bounds(glm::ivec3 pos, glm::ivec3 size);
    static size_t idx(glm::ivec3 pos, glm::ivec3 size);
    static bool is_free(const std::vector<Voxel>& voxels, glm::ivec3 pos, glm::ivec3 size);

    static bool solid_from(const std::vector<Voxel>& self, 
                           const std::array<std::shared_ptr<const std::vector<Voxel>>,6>& nb, 
                           glm::ivec3 pos, glm::ivec3 size);

    void draw(RenderState state) override;
};

