#include "chunk.h"
#include "voxel_grid.h"

Chunk::Chunk(glm::ivec3 size, glm::vec3 voxel_size) {
    this->size = size;
    this->voxel_size = voxel_size;

    auto v = std::make_shared<std::vector<Voxel>>();
    v->resize(size.x * size.y * size.z);
    voxels = v;

    this->vertex_layout = new VertexLayout();
    vertex_layout->add({
        0, 3, GL_FLOAT, GL_FALSE,
        9 * sizeof(float),
        0
    });
    vertex_layout->add({
        1, 3, GL_FLOAT, GL_FALSE,
        9 * sizeof(float),
        3 * sizeof(float)
    });
    vertex_layout->add({
        2, 3, GL_FLOAT, GL_FALSE,
        9 * sizeof(float),
        6 * sizeof(float)
    });

    vertices.reserve(size.x * size.y * size.z * 4 * 6 * 9);
    indices.reserve(size.x * size.y * size.z * 6 * 6);
    update_mesh();
}

Chunk::~Chunk() {
    delete mesh;
    delete vertex_layout;
}

// void Chunk::set_voxel(glm::ivec3 pos, Voxel voxel) {
//     if(!in_bounds(pos))
//         return;
//     voxels[idx(pos.x, pos.y, pos.z)] = voxel;
// }

void Chunk::update_necessary_mesh() {
    for (int i = 0; i < voxels_to_update.size(); i++) {

    }
}


// void Chunk::clear() {
//     for (int x = 0; x < size.x; x++)
//         for (int y = 0; y < size.y; y++)
//             for (int z = 0; z < size.z; z++)
//                 voxels[idx(x, y, z)].visible = false;
// }

void Chunk::update_mesh() {
    vertices.clear();
    indices.clear();

    auto v = std::atomic_load(&voxels);

    for (int x = 0; x < size.x; x++) {
        for (int y = 0; y < size.y; y++) {
            for (int z = 0; z < size.z; z++) {
                if (is_free({x, y, z}))
                    continue;
                size_t id = idx(x, y, z);
                glm::vec3 voxel_color = (*v)[id].color;
                
                if (is_free({x-1, y, z}))
                    add_left_voxel_face({x, y, z}, voxel_color);
                if (is_free({x, y, z-1})) 
                    add_back_voxel_face({x, y, z}, voxel_color);
                if (is_free({x, y+1, z}))
                    add_top_voxel_face({x, y, z}, voxel_color);
                if (is_free({x+1, y, z}))
                    add_right_voxel_face({x, y, z}, voxel_color);
                if (is_free({x, y, z+1}))
                    add_front_voxel_face({x, y, z}, voxel_color);
                if (is_free({x, y-1, z}))
                    add_bottom_voxel_face({x, y, z}, voxel_color);
            }
        }
    }
    if (vertices.empty() || indices.empty())
        return;

    
    if (!mesh) {
        // std::cout << mesh << std::endl;
        this->mesh = new Mesh(vertices, indices, vertex_layout);
    }
    else {
        this->mesh->update(vertices, indices);
    }
}

void Chunk::update_mesh(VoxelGrid* voxel_grid, glm::ivec3 chunk_pos) {
    build_mesh_cpu(voxel_grid, chunk_pos);
    upload_mesh_gpu();
}

void Chunk::build_mesh_cpu(VoxelGrid* voxel_grid, glm::ivec3 chunk_pos) {
    auto v = std::atomic_load(&voxels);

    vertices.clear();
    indices.clear();
    for (int lx = 0; lx < size.x; lx++) {
        for (int ly = 0; ly < size.y; ly++) {
            for (int lz = 0; lz < size.z; lz++) {
                int x = lx + chunk_pos.x * voxel_grid->chunk_size.x;
                int y = ly + chunk_pos.y * voxel_grid->chunk_size.y;
                int z = lz + chunk_pos.z * voxel_grid->chunk_size.z;

                if (voxel_grid->is_voxel_free({x, y, z}))
                    continue;
                    
                size_t id = idx(lx, ly, lz);
                glm::vec3 voxel_color = (*v)[id].color;
                
                if (voxel_grid->is_voxel_free({x-1, y, z}))
                    add_left_voxel_face({lx, ly, lz}, voxel_color);
                if (voxel_grid->is_voxel_free({x, y, z-1})) 
                    add_back_voxel_face({lx, ly, lz}, voxel_color);
                if (voxel_grid->is_voxel_free({x, y+1, z}))
                    add_top_voxel_face({lx, ly, lz}, voxel_color);
                if (voxel_grid->is_voxel_free({x+1, y, z}))
                    add_right_voxel_face({lx, ly, lz}, voxel_color);
                if (voxel_grid->is_voxel_free({x, y, z+1}))
                    add_front_voxel_face({lx, ly, lz}, voxel_color);
                if (voxel_grid->is_voxel_free({x, y-1, z}))
                    add_bottom_voxel_face({lx, ly, lz}, voxel_color);
                }
            }
        }
}

MeshData Chunk::build_mesh_cpu_detahed(VoxelGrid* voxel_grid, glm::ivec3 chunk_pos) const{
    MeshData out;
    out.vertices.reserve(size.x * size.y * size.z * 4 * 6 * 9);
    out.indices.reserve(size.x * size.y * size.z * 6 * 6);

    auto v = std::atomic_load(&voxels);

    for (int lx = 0; lx < size.x; lx++) {
        for (int ly = 0; ly < size.y; ly++) {
            for (int lz = 0; lz < size.z; lz++) {
                // if (is_free({x, y, z}))
                //     continue;
                int x = lx + chunk_pos.x * voxel_grid->chunk_size.x;
                int y = ly + chunk_pos.y * voxel_grid->chunk_size.y;
                int z = lz + chunk_pos.z * voxel_grid->chunk_size.z;

                if (voxel_grid->is_voxel_free({x, y, z}))
                    continue;
                size_t id = idx(lx, ly, lz);
                glm::vec3 voxel_color = (*v)[id].color;
                
                // if (is_free({x-1, y, z}))
                //     add_left_voxel_face(out.vertices, out.indices, {x, y, z}, voxel_color);
                // if (is_free({x, y, z-1}))
                //     add_back_voxel_face(out.vertices, out.indices, {x, y, z}, voxel_color);
                // if (is_free({x, y+1, z}))
                //     add_top_voxel_face(out.vertices, out.indices, {x, y, z}, voxel_color);
                // if (is_free({x+1, y, z}))
                //     add_right_voxel_face(out.vertices, out.indices, {x, y, z}, voxel_color);
                // if (is_free({x, y, z+1}))
                //     add_front_voxel_face(out.vertices, out.indices, {x, y, z}, voxel_color);
                // if (is_free({x, y-1, z}))
                //     add_bottom_voxel_face(out.vertices, out.indices, {x, y, z}, voxel_color);

                if (voxel_grid->is_voxel_free({x-1, y, z}))
                    add_left_voxel_face(out.vertices, out.indices, {lx, ly, lz}, voxel_color);
                if (voxel_grid->is_voxel_free({x, y, z-1})) 
                    add_back_voxel_face(out.vertices, out.indices, {lx, ly, lz}, voxel_color);
                if (voxel_grid->is_voxel_free({x, y+1, z}))
                    add_top_voxel_face(out.vertices, out.indices, {lx, ly, lz}, voxel_color);
                if (voxel_grid->is_voxel_free({x+1, y, z}))
                    add_right_voxel_face(out.vertices, out.indices, {lx, ly, lz}, voxel_color);
                if (voxel_grid->is_voxel_free({x, y, z+1}))
                    add_front_voxel_face(out.vertices, out.indices, {lx, ly, lz}, voxel_color);
                if (voxel_grid->is_voxel_free({x, y-1, z}))
                    add_bottom_voxel_face(out.vertices, out.indices, {lx, ly, lz}, voxel_color);
                }
            }
        }
    return out;
}



// MeshData Chunk::build_mesh_cpu_detahed(std::shared_ptr<std::vector<Voxel>> voxels, glm::ivec3 chunk_pos, glm::ivec3 chunk_size) const{
//     MeshData out;
//     out.vertices.reserve(size.x * size.y * size.z * 4 * 6 * 9);
//     out.indices.reserve(size.x * size.y * size.z * 6 * 6);

//     auto v = std::atomic_load(&voxels);

//     for (int lx = 0; lx < size.x; lx++) {
//         for (int ly = 0; ly < size.y; ly++) {
//             for (int lz = 0; lz < size.z; lz++) {
//                 // if (is_free({x, y, z}))
//                 //     continue;
//                 int x = lx + chunk_pos.x * chunk_size.x;
//                 int y = ly + chunk_pos.y * chunk_size.y;
//                 int z = lz + chunk_pos.z * chunk_size.z;

//                 if (voxel_grid->is_voxel_free({x, y, z}))
//                     continue;
//                 size_t id = idx(lx, ly, lz);
//                 glm::vec3 voxel_color = (*v)[id].color;
                
//                 // if (is_free({x-1, y, z}))
//                 //     add_left_voxel_face(out.vertices, out.indices, {x, y, z}, voxel_color);
//                 // if (is_free({x, y, z-1}))
//                 //     add_back_voxel_face(out.vertices, out.indices, {x, y, z}, voxel_color);
//                 // if (is_free({x, y+1, z}))
//                 //     add_top_voxel_face(out.vertices, out.indices, {x, y, z}, voxel_color);
//                 // if (is_free({x+1, y, z}))
//                 //     add_right_voxel_face(out.vertices, out.indices, {x, y, z}, voxel_color);
//                 // if (is_free({x, y, z+1}))
//                 //     add_front_voxel_face(out.vertices, out.indices, {x, y, z}, voxel_color);
//                 // if (is_free({x, y-1, z}))
//                 //     add_bottom_voxel_face(out.vertices, out.indices, {x, y, z}, voxel_color);

//                 if (voxel_grid->is_voxel_free({x-1, y, z}))
//                     add_left_voxel_face(out.vertices, out.indices, {lx, ly, lz}, voxel_color);
//                 if (voxel_grid->is_voxel_free({x, y, z-1})) 
//                     add_back_voxel_face(out.vertices, out.indices, {lx, ly, lz}, voxel_color);
//                 if (voxel_grid->is_voxel_free({x, y+1, z}))
//                     add_top_voxel_face(out.vertices, out.indices, {lx, ly, lz}, voxel_color);
//                 if (voxel_grid->is_voxel_free({x+1, y, z}))
//                     add_right_voxel_face(out.vertices, out.indices, {lx, ly, lz}, voxel_color);
//                 if (voxel_grid->is_voxel_free({x, y, z+1}))
//                     add_front_voxel_face(out.vertices, out.indices, {lx, ly, lz}, voxel_color);
//                 if (voxel_grid->is_voxel_free({x, y-1, z}))
//                     add_bottom_voxel_face(out.vertices, out.indices, {lx, ly, lz}, voxel_color);
//                 }
//             }
//         }
//     return out;
// }


MeshData Chunk::build_mesh_from_snapshot(const std::vector<Voxel> &voxels, glm::ivec3 chunk_pos, glm::ivec3 chunk_size) {
    const int num_vertex_components = 9;
    
    MeshData out;
    out.vertices.reserve(chunk_size.x * chunk_size.y * chunk_size.z * 4 * 6 * num_vertex_components);
    out.indices.reserve(chunk_size.x * chunk_size.y * chunk_size.z * 6 * 6);
    
    for (int lx = 0; lx < chunk_size.x; lx++) {
        for (int ly = 0; ly < chunk_size.y; ly++) {
            for (int lz = 0; lz < chunk_size.z; lz++) {
                // if (is_free({x, y, z}))
                //     continue;
                // int x = lx + chunk_pos.x * chunk_size.x;
                // int y = ly + chunk_pos.y * chunk_size.y;
                // int z = lz + chunk_pos.z * chunk_size.z;

                int x = lx;
                int y = ly;
                int z = lz;


                if (Chunk::is_free(voxels, {x, y, z}, chunk_size))
                    continue;
                size_t id = Chunk::idx(lx, ly, lz, chunk_size);
                glm::vec3 voxel_color = voxels[id].color;
                
                if (Chunk::is_free(voxels, {x-1, y, z}, chunk_size))
                    Chunk::add_left_voxel_face(voxels, out.vertices, out.indices, {x, y, z}, voxel_color);
                if (Chunk::is_free(voxels, {x, y, z-1}, chunk_size))
                    Chunk::add_back_voxel_face(voxels, out.vertices, out.indices, {x, y, z}, voxel_color);
                if (Chunk::is_free(voxels, {x, y+1, z}, chunk_size))
                    Chunk::add_top_voxel_face(voxels, out.vertices, out.indices, {x, y, z}, voxel_color);
                if (Chunk::is_free(voxels, {x+1, y, z}, chunk_size))
                    Chunk::add_right_voxel_face(voxels, out.vertices, out.indices, {x, y, z}, voxel_color);
                if (Chunk::is_free(voxels, {x, y, z+1}, chunk_size))
                    Chunk::add_front_voxel_face(voxels, out.vertices, out.indices, {x, y, z}, voxel_color);
                if (Chunk::is_free(voxels, {x, y-1, z}, chunk_size))
                    Chunk::add_bottom_voxel_face(voxels, out.vertices, out.indices, {x, y, z}, voxel_color);

                // if (voxel_grid->is_voxel_free({x-1, y, z}))
                //     add_left_voxel_face(out.vertices, out.indices, {lx, ly, lz}, voxel_color);
                // if (voxel_grid->is_voxel_free({x, y, z-1})) 
                //     add_back_voxel_face(out.vertices, out.indices, {lx, ly, lz}, voxel_color);
                // if (voxel_grid->is_voxel_free({x, y+1, z}))
                //     add_top_voxel_face(out.vertices, out.indices, {lx, ly, lz}, voxel_color);
                // if (voxel_grid->is_voxel_free({x+1, y, z}))
                //     add_right_voxel_face(out.vertices, out.indices, {lx, ly, lz}, voxel_color);
                // if (voxel_grid->is_voxel_free({x, y, z+1}))
                //     add_front_voxel_face(out.vertices, out.indices, {lx, ly, lz}, voxel_color);
                // if (voxel_grid->is_voxel_free({x, y-1, z}))
                //     add_bottom_voxel_face(out.vertices, out.indices, {lx, ly, lz}, voxel_color);
                // }
            }
        }
    }

    return out;    
}

void Chunk::upload_mesh_gpu() {
    if (vertices.empty() || indices.empty())
        return;
    if (!mesh)
        this->mesh = new Mesh(vertices, indices, vertex_layout);
    else
        this->mesh->update(vertices, indices);
}

void Chunk::upload_mesh_gpu(MeshData mesh_data) {
    vertices = std::move(mesh_data.vertices);
    indices = std::move(mesh_data.indices);

    if (vertices.empty() || indices.empty())
        return;
    if (!mesh)
        this->mesh = new Mesh(vertices, indices, vertex_layout);
    else
        this->mesh->update(vertices, indices);
}


bool Chunk::is_free(glm::ivec3 pos) const {
    if (!in_bounds(pos))
        return true;
    if (!get_voxel(pos).visible)
        return true;
    return false;
}

bool Chunk::is_free(const std::vector<Voxel>& voxels, glm::ivec3 pos, glm::ivec3 chunk_size) {
    if (!in_bounds(voxels, pos, chunk_size))
        return true;
    if (!get_voxel(voxels, pos, chunk_size).visible)
        return true;
    return false;
}

// bool Chunk::is_free(std::shared_ptr<std::vector<Voxel>> voxels, glm::ivec3 pos) const {
//     if (!in_bounds(pos))
//         return true;
//     if (!get_voxel(pos).visible)
//         return true;
//     return false;
// }

bool Chunk::in_bounds(glm::ivec3 pos) const {
    if (pos.x < 0 || pos.x >= size.x)
        return false;
    if (pos.y < 0 || pos.y >= size.y)
        return false;
    if (pos.z < 0 || pos.z >= size.z)
        return false;
    return true;
}

bool Chunk::in_bounds(const std::vector<Voxel>& voxels, glm::ivec3 pos, glm::ivec3 chunk_size) {
    if (pos.x < 0 || pos.x >= chunk_size.x)
        return false;
    if (pos.y < 0 || pos.y >= chunk_size.y)
        return false;
    if (pos.z < 0 || pos.z >= chunk_size.z)
        return false;
    return true;
}

Voxel Chunk::get_voxel(glm::ivec3 pos) const {
    auto v = std::atomic_load(&voxels);
    return (*v)[idx(pos.x, pos.y, pos.z)];
}

Voxel Chunk::get_voxel(const std::vector<Voxel>& voxels, glm::ivec3 pos, glm::ivec3 chunk_size) {
    return voxels[idx(pos.x, pos.y, pos.z, chunk_size)];
}

glm::vec3 Chunk::get_vertex_pos(glm::vec3 voxel_pos, glm::vec3 corner_pos){
    return voxel_pos + corner_pos;
}

void Chunk::add_vertex(glm::vec3 pos, glm::vec3 normal, glm::vec3 color) {
    vertices.push_back(pos.x);
    vertices.push_back(pos.y);
    vertices.push_back(pos.z);
    vertices.push_back(normal.x);
    vertices.push_back(normal.y);
    vertices.push_back(normal.z);
    vertices.push_back(color.x);
    vertices.push_back(color.y);
    vertices.push_back(color.z);
}

void Chunk::add_vertex(std::vector<float> &vertices, glm::vec3 pos, glm::vec3 normal, glm::vec3 color){
    vertices.push_back(pos.x);
    vertices.push_back(pos.y);
    vertices.push_back(pos.z);
    vertices.push_back(normal.x);
    vertices.push_back(normal.y);
    vertices.push_back(normal.z);
    vertices.push_back(color.x);
    vertices.push_back(color.y);
    vertices.push_back(color.z);
}

void Chunk::add_voxel_face(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, size_t first_idx, glm::vec3 normal, glm::vec3 color){
    add_vertex(vertices, v0, normal, color);
    add_vertex(vertices, v1, normal, color);
    add_vertex(vertices, v2, normal, color);
    add_vertex(vertices, v3, normal, color);

    indices.push_back(first_idx);
    indices.push_back(first_idx+1);
    indices.push_back(first_idx+2);

    indices.push_back(first_idx+2);
    indices.push_back(first_idx+3);
    indices.push_back(first_idx);
}

void Chunk::add_voxel_face(std::vector<float> &vertices, std::vector<unsigned int> &indices, glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, size_t first_idx, glm::vec3 normal, glm::vec3 color) {
    add_vertex(vertices, v0, normal, color);
    add_vertex(vertices, v1, normal, color);
    add_vertex(vertices, v2, normal, color);
    add_vertex(vertices, v3, normal, color);

    indices.push_back(first_idx);
    indices.push_back(first_idx+1);
    indices.push_back(first_idx+2);

    indices.push_back(first_idx+2);
    indices.push_back(first_idx+3);
    indices.push_back(first_idx);
}

void Chunk::add_voxel_face(glm::ivec3 pos, glm::ivec3 corner0, glm::ivec3 corner1, glm::ivec3 corner2, glm::ivec3 corner3, glm::vec3 normal, glm::vec3 color) {
    int first_idx = (int)vertices.size() / num_vertex_components;
    float x = pos.x;
    float y = pos.y;
    float z = pos.z;
    glm::vec3 v0 = get_vertex_pos({x, y, z}, corner0);
    glm::vec3 v1 = get_vertex_pos({x, y, z}, corner1);
    glm::vec3 v2 = get_vertex_pos({x, y, z}, corner2);
    glm::vec3 v3 = get_vertex_pos({x, y, z}, corner3);

    add_voxel_face(v0, v1, v2, v3, first_idx, normal, color);
}

void Chunk::add_voxel_face(std::vector<float> &vertices, std::vector<unsigned int> &indices, glm::ivec3 pos, glm::ivec3 corner0, glm::ivec3 corner1, glm::ivec3 corner2, glm::ivec3 corner3, glm::vec3 normal, glm::vec3 color) {
    const int num_vertex_components = 9;

    int first_idx = (int)vertices.size() / num_vertex_components;
    float x = pos.x;
    float y = pos.y;
    float z = pos.z;
    glm::vec3 v0 = get_vertex_pos({x, y, z}, corner0);
    glm::vec3 v1 = get_vertex_pos({x, y, z}, corner1);
    glm::vec3 v2 = get_vertex_pos({x, y, z}, corner2);
    glm::vec3 v3 = get_vertex_pos({x, y, z}, corner3);

    add_voxel_face(vertices, indices, v0, v1, v2, v3, first_idx, normal, color);
}

void Chunk::add_left_voxel_face(glm::ivec3 pos, glm::vec3 color) {
    glm::vec3 normal = {-1, 0, 0};
    add_voxel_face(pos, {0, 0, 0}, {0, 1, 0}, {0, 1, 1}, {0, 0, 1}, normal, color);
}

void Chunk::add_back_voxel_face(glm::ivec3 pos, glm::vec3 color) {
    glm::vec3 normal = {0, 0, -1};
    add_voxel_face(pos, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}, normal, color);
}

void Chunk::add_right_voxel_face(glm::ivec3 pos, glm::vec3 color) {
    glm::vec3 normal = {1, 0, 0};
    add_voxel_face(pos, {1, 0, 1}, {1, 1, 1}, {1, 1, 0}, {1, 0, 0}, normal, color);
}

void Chunk::add_front_voxel_face(glm::ivec3 pos, glm::vec3 color) {
    glm::vec3 normal = {0, 0, 1};
    add_voxel_face(pos, {0, 0, 1}, {0, 1, 1}, {1, 1, 1}, {1, 0, 1}, normal, color);
}

void Chunk::add_top_voxel_face(glm::ivec3 pos, glm::vec3 color) {
    glm::vec3 normal = {0, 1, 0};
    add_voxel_face(pos,{0, 1, 1}, {0, 1, 0}, {1, 1, 0}, {1, 1, 1}, normal, color);
}

void Chunk::add_bottom_voxel_face(glm::ivec3 pos, glm::vec3 color) {
    glm::vec3 normal = {0, -1, 0}; 
    add_voxel_face(pos,{0, 0, 0}, {0, 0, 1}, {1, 0, 1}, {1, 0, 0}, normal, color);
}



void Chunk::add_left_voxel_face(std::vector<float> &vertices, std::vector<unsigned int> &indices, glm::ivec3 pos, glm::vec3 color) const {
    glm::vec3 normal = {-1, 0, 0};
    add_voxel_face(vertices, indices, pos, {0, 0, 0}, {0, 1, 0}, {0, 1, 1}, {0, 0, 1}, normal, color);
}

void Chunk::add_back_voxel_face(std::vector<float> &vertices, std::vector<unsigned int> &indices, glm::ivec3 pos, glm::vec3 color) const {
    glm::vec3 normal = {0, 0, -1};
    add_voxel_face(vertices, indices, pos, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}, normal, color);
}

void Chunk::add_right_voxel_face(std::vector<float> &vertices, std::vector<unsigned int> &indices, glm::ivec3 pos, glm::vec3 color) const {
    glm::vec3 normal = {1, 0, 0};
    add_voxel_face(vertices, indices, pos, {1, 0, 1}, {1, 1, 1}, {1, 1, 0}, {1, 0, 0}, normal, color);
}

void Chunk::add_front_voxel_face(std::vector<float> &vertices, std::vector<unsigned int> &indices, glm::ivec3 pos, glm::vec3 color) const {
    glm::vec3 normal = {0, 0, 1};
    add_voxel_face(vertices, indices, pos, {0, 0, 1}, {0, 1, 1}, {1, 1, 1}, {1, 0, 1}, normal, color);
}

void Chunk::add_top_voxel_face(std::vector<float> &vertices, std::vector<unsigned int> &indices, glm::ivec3 pos, glm::vec3 color) const {
    glm::vec3 normal = {0, 1, 0};
    add_voxel_face(vertices, indices, pos,{0, 1, 1}, {0, 1, 0}, {1, 1, 0}, {1, 1, 1}, normal, color);
}

void Chunk::add_bottom_voxel_face(std::vector<float> &vertices, std::vector<unsigned int> &indices, glm::ivec3 pos, glm::vec3 color) const {
    glm::vec3 normal = {0, -1, 0}; 
    add_voxel_face(vertices, indices, pos,{0, 0, 0}, {0, 0, 1}, {1, 0, 1}, {1, 0, 0}, normal, color);
}



void Chunk::add_left_voxel_face(const std::vector<Voxel> &voxels, std::vector<float> &vertices, std::vector<unsigned int> &indices, glm::ivec3 pos, glm::vec3 color) {
    glm::vec3 normal = {-1, 0, 0};
    add_voxel_face(vertices, indices, pos, {0, 0, 0}, {0, 1, 0}, {0, 1, 1}, {0, 0, 1}, normal, color);
}

void Chunk::add_back_voxel_face(const std::vector<Voxel> &voxels, std::vector<float> &vertices, std::vector<unsigned int> &indices, glm::ivec3 pos, glm::vec3 color) {
    glm::vec3 normal = {0, 0, -1};
    add_voxel_face(vertices, indices, pos, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}, normal, color);
}

void Chunk::add_right_voxel_face(const std::vector<Voxel> &voxels, std::vector<float> &vertices, std::vector<unsigned int> &indices, glm::ivec3 pos, glm::vec3 color) {
    glm::vec3 normal = {1, 0, 0};
    add_voxel_face(vertices, indices, pos, {1, 0, 1}, {1, 1, 1}, {1, 1, 0}, {1, 0, 0}, normal, color);
}

void Chunk::add_front_voxel_face(const std::vector<Voxel> &voxels, std::vector<float> &vertices, std::vector<unsigned int> &indices, glm::ivec3 pos, glm::vec3 color) {
    glm::vec3 normal = {0, 0, 1};
    add_voxel_face(vertices, indices, pos, {0, 0, 1}, {0, 1, 1}, {1, 1, 1}, {1, 0, 1}, normal, color);
}

void Chunk::add_top_voxel_face(const std::vector<Voxel> &voxels, std::vector<float> &vertices, std::vector<unsigned int> &indices, glm::ivec3 pos, glm::vec3 color) {
    glm::vec3 normal = {0, 1, 0};
    add_voxel_face(vertices, indices, pos,{0, 1, 1}, {0, 1, 0}, {1, 1, 0}, {1, 1, 1}, normal, color);
}

void Chunk::add_bottom_voxel_face(const std::vector<Voxel> &voxels, std::vector<float> &vertices, std::vector<unsigned int> &indices, glm::ivec3 pos, glm::vec3 color) {
    glm::vec3 normal = {0, -1, 0}; 
    add_voxel_face(vertices, indices, pos,{0, 0, 0}, {0, 0, 1}, {1, 0, 1}, {1, 0, 0}, normal, color);
}


size_t Chunk::idx(int x,int y,int z) const {
    return (size_t)x + (size_t)size.x * ((size_t)y + (size_t)size.y * (size_t)z);
}

size_t Chunk::idx(int x,int y,int z, glm::ivec3 chunk_size) {
    return (size_t)x + (size_t)chunk_size.x * ((size_t)y + (size_t)chunk_size.y * (size_t)z);
}

void Chunk::draw(RenderState state) {
    if (vertices.empty() || indices.empty())
        return;
    state.transform *= get_model_matrix();
    mesh->draw(state);
}