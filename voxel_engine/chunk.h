#pragma once
#include "../mesh.h"
#include "voxel.h"
#include "iostream"


// struct Vertex {
//     float x;
//     float y;
//     float z;
//     float normal_x;
//     float normal_y;
//     float normal_z;
// };

class Chunk : public Drawable, public Transformable {
public:
    glm::vec3 size;
    glm::vec3 voxel_size;
    Voxel**** voxels;
    Mesh* mesh;
    Chunk(glm::vec3 size, glm::vec3 voxel_size);
    bool is_free(glm::vec3 pos);
    bool in_bounds(glm::vec3 pos);
    Voxel* get_voxel(glm::vec3 pos);
    glm::vec3 get_vertex_pos(glm::vec3 voxel_pos, glm::vec3 corner_pos);
    void add_vertex(std::vector<float>& vertices, glm::vec3 pos, glm::vec3 normal, glm::vec3 color);
    void draw(RenderState state) override;
};
