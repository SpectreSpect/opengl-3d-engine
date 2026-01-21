#include "chunk.h"
#include "voxel_grid.h"

Chunk::Chunk(glm::ivec3 size, glm::vec3 voxel_size) {
    this->size = size;
    this->voxel_size = voxel_size;

    voxels.resize(size.x * size.y * size.z);

    // size_i = glm::ivec()
    // Voxel* ceat[(int)size.x][(int)size.y][(int)size.z];
    // (*cat)[0][0][0] = new Voxel;

    // voxels = new Voxel***[(int)size.x];
    // for (int x = 0; x < size.x; x++) {
    //     // voxels[x] = new Voxel**[(int)size.y];
    //     for (int y = 0; y < size.y; y++) {
    //         // voxels[x][y] = new Voxel*[(int)size.z];
    //         for (int z = 0; z < size.z; z++) {
    //             // voxels[x][y][z] = new Voxel();
    //             float pi = 3.14;
    //             float norm_x = (float)x / (float)size.x;
    //             float norm_z = (float)z / (float)size.z;
                
    //             float value = (float)y / size.y;
    //             float wave_1 = (sin(norm_x * 2 * pi + 1) + cos(norm_z * 2 * pi * 2 + 1)) / 2.0 + 1.0;
    //             float wave_2 = (sin(norm_x * 5 * pi + 2.5) + cos(norm_z * 5 * pi * 2 + 4.2345)) / 2.0 + 1.0;
    //             float wave_3 = (sin(norm_x / 2.0) + cos(norm_z / 2.0)) / 2.0 + 1.0;
    //             // float wave_3 = (sin(norm_x * 2 * pi * 6 + 1.111) + cos(norm_z * 8 * pi + 2.645)) / 2.0 + 1.0;
    //             float threshold = (wave_1 + wave_2/3.0 + wave_3/3.0) / 3.0;
    //             // float threshold = (wave_1/2.0 + wave_2/2.0) / 3.0;

    //             glm::vec3 sand_color = {0.921568627, 0.811764706, 0.662745098};
    //             glm::vec3 water_color = {0.282352941, 0.541176471, 1.0};
    //             glm::vec3 grass_color = {0.023529412, 0.658823529, 0.443137255};
    //             glm::vec3 rock_color = {0.650980392, 0.650980392, 0.650980392};
    //             glm::vec3 snow_color = {1.0, 1.0, 1.0};

    //             float norm_y = (float)y / (float)size.y;

    //             size_t id = idx(x, y, z);

    //             if (norm_y <= 0.2f)
    //                 voxels[id].color = water_color;
    //             else if (norm_y > 0.2f && norm_y <= 0.3f)
    //                 voxels[id].color = sand_color;
    //             else if (norm_y > 0.3f && norm_y <= 0.5f)
    //                 voxels[id].color = grass_color;
    //             else if (norm_y > 0.5f && norm_y < 0.8f)
    //                 voxels[id].color = rock_color;
    //             else if (norm_y > 0.8f)
    //                 voxels[id].color = snow_color;

    //             if (value < threshold)
    //                 voxels[id].visible = true;
    //         }
    //     }
    // }

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

void Chunk::set_voxel(glm::ivec3 pos, Voxel voxel) {
    if(!in_bounds(pos))
        return;
    voxels[idx(pos.x, pos.y, pos.z)] = voxel;
}

void Chunk::update_necessary_mesh() {
    for (int i = 0; i < voxels_to_update.size(); i++) {

    }
}

void Chunk::clear() {
    for (int x = 0; x < size.x; x++)
        for (int y = 0; y < size.y; y++)
            for (int z = 0; z < size.z; z++)
                voxels[idx(x, y, z)].visible = false;
}

void Chunk::update_mesh() {
    vertices.clear();
    indices.clear();

    for (int x = 0; x < size.x; x++) {
        for (int y = 0; y < size.y; y++) {
            for (int z = 0; z < size.z; z++) {
                if (is_free({x, y, z}))
                    continue;
                size_t id = idx(x, y, z);
                glm::vec3 voxel_color = voxels[id].color;
                
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
                glm::vec3 voxel_color = voxels[id].color;
                
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

    // for (int lx = 0; lx < size.x; lx++) {
    //     for (int ly = 0; ly < size.y; ly++) {
    //         for (int lz = 0; lz < size.z; lz++) {
    //             int x = lx + chunk_pos.x * voxel_grid->chunk_size.x;
    //             int y = ly + chunk_pos.y * voxel_grid->chunk_size.y;
    //             int z = lz + chunk_pos.z * voxel_grid->chunk_size.z;

    //             if (voxel_grid->is_voxel_free({x, y, z}))
    //                 continue;
                    
    //             size_t id = idx(lx, ly, lz);
    //             glm::vec3 voxel_color = voxels[id].color;
                
    //             if (voxel_grid->is_voxel_free({x-1, y, z}))
    //                 add_left_voxel_face(out.vertices, out.indices, {lx, ly, lz}, voxel_color);
    //             if (voxel_grid->is_voxel_free({x, y, z-1})) 
    //                 add_back_voxel_face(out.vertices, out.indices, {lx, ly, lz}, voxel_color);
    //             if (voxel_grid->is_voxel_free({x, y+1, z}))
    //                 add_top_voxel_face(out.vertices, out.indices, {lx, ly, lz}, voxel_color);
    //             if (voxel_grid->is_voxel_free({x+1, y, z}))
    //                 add_right_voxel_face(out.vertices, out.indices, {lx, ly, lz}, voxel_color);
    //             if (voxel_grid->is_voxel_free({x, y, z+1}))
    //                 add_front_voxel_face(out.vertices, out.indices, {lx, ly, lz}, voxel_color);
    //             if (voxel_grid->is_voxel_free({x, y-1, z}))
    //                 add_bottom_voxel_face(out.vertices, out.indices, {lx, ly, lz}, voxel_color);
    //             }
    //         }
    //     }

    for (int x = 0; x < size.x; x++) {
        for (int y = 0; y < size.y; y++) {
            for (int z = 0; z < size.z; z++) {
                if (is_free({x, y, z}))
                    continue;
                size_t id = idx(x, y, z);
                glm::vec3 voxel_color = voxels[id].color;
                
                if (is_free({x-1, y, z}))
                    add_left_voxel_face(out.vertices, out.indices, {x, y, z}, voxel_color);
                if (is_free({x, y, z-1})) 
                    add_back_voxel_face(out.vertices, out.indices, {x, y, z}, voxel_color);
                if (is_free({x, y+1, z}))
                    add_top_voxel_face(out.vertices, out.indices, {x, y, z}, voxel_color);
                if (is_free({x+1, y, z}))
                    add_right_voxel_face(out.vertices, out.indices, {x, y, z}, voxel_color);
                if (is_free({x, y, z+1}))
                    add_front_voxel_face(out.vertices, out.indices, {x, y, z}, voxel_color);
                if (is_free({x, y-1, z}))
                    add_bottom_voxel_face(out.vertices, out.indices, {x, y, z}, voxel_color);
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

bool Chunk::in_bounds(glm::ivec3 pos) const {
    if (pos.x < 0 || pos.x >= size.x)
        return false;
    if (pos.y < 0 || pos.y >= size.y)
        return false;
    if (pos.z < 0 || pos.z >= size.z)
        return false;
    return true;
}

Voxel Chunk::get_voxel(glm::ivec3 pos) const {
    return voxels[idx(pos.x, pos.y, pos.z)];
}

glm::vec3 Chunk::get_vertex_pos(glm::vec3 voxel_pos, glm::vec3 corner_pos) const{
    // float relative_voxel_size = 1.0f / std::max(size.x, size.y, size.z);
    // float relative_voxel_size = 1.0f;
    
    // glm::vec3 vertex_pos = voxel_pos * relative_voxel_size + corner_pos * relative_voxel_size;
    // glm::vec3 vertex_pos = voxel_pos + corner_pos;
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

void Chunk::add_vertex(std::vector<float> &vertices, glm::vec3 pos, glm::vec3 normal, glm::vec3 color) const{
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

void Chunk::add_voxel_face(std::vector<float> &vertices, std::vector<unsigned int> &indices, glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, size_t first_idx, glm::vec3 normal, glm::vec3 color) const{
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

void Chunk::add_voxel_face(std::vector<float> &vertices, std::vector<unsigned int> &indices, glm::ivec3 pos, glm::ivec3 corner0, glm::ivec3 corner1, glm::ivec3 corner2, glm::ivec3 corner3, glm::vec3 normal, glm::vec3 color) const {
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


size_t Chunk::idx(int x,int y,int z) const {
    return (size_t)x + (size_t)size.x * ((size_t)y + (size_t)size.y * (size_t)z);
}

void Chunk::draw(RenderState state) {
    if (vertices.empty() || indices.empty())
        return;
    state.transform *= get_model_matrix();
    mesh->draw(state);
}