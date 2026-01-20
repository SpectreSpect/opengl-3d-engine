#pragma once
#include "../mesh.h"
#include "voxel.h"
#include "iostream"


// struct Vertex {
//     float pos_x;
//     float pos_y;
//     float pos_z;
//     float normal_x;
//     float normal_y;
//     float normal_z;
//     float color_r;
//     float color_g;
//     float color_b;
// };

class Chunk : public Drawable, public Transformable {
public:
    glm::ivec3 size;
    glm::vec3 voxel_size;
    VertexLayout* vertex_layout;
    // Voxel**** voxels;
    std::vector<Voxel> voxels;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    std::vector<glm::ivec3> voxels_to_update;
    const int num_vertex_components = 9;
    bool TEMP = false;
    // size_t num_vertices = 0;
    // size_t num_indices = 0;
    Mesh* mesh;
    Chunk(glm::ivec3 size, glm::vec3 voxel_size);
    void update_mesh(VertexLayout* vertex_layout = nullptr);
    void update_necessary_mesh();
    void set_voxel(glm::ivec3 pos, Voxel voxel);
    bool is_free(glm::ivec3 pos);
    bool in_bounds(glm::ivec3 pos);
    Voxel get_voxel(glm::ivec3 pos);
    glm::vec3 get_vertex_pos(glm::vec3 voxel_pos, glm::vec3 corner_pos);
    void add_vertex(glm::vec3 pos, glm::vec3 normal, glm::vec3 color);
    void add_voxel_face(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, size_t first_idx, glm::vec3 normal, glm::vec3 color);
    void add_voxel_face(glm::ivec3 pos, glm::ivec3 corner0, glm::ivec3 corner1, glm::ivec3 corner2, glm::ivec3 corner3, glm::vec3 normal, glm::vec3 color);
    void add_left_voxel_face(glm::ivec3 pos, glm::vec3 color);
    void add_back_voxel_face(glm::ivec3 pos, glm::vec3 color);
    void add_right_voxel_face(glm::ivec3 pos, glm::vec3 color);
    void add_front_voxel_face(glm::ivec3 pos, glm::vec3 color);
    void add_top_voxel_face(glm::ivec3 pos, glm::vec3 color);
    void add_bottom_voxel_face(glm::ivec3 pos, glm::vec3 color);
    size_t idx(int x,int y,int z) const;
    void draw(RenderState state) override;
};

