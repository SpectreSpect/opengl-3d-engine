#include "chunk.h"

Chunk::Chunk(glm::vec3 size, glm::vec3 voxel_size) {
    this->size = size;
    this->voxel_size = voxel_size;
    Voxel* ceat[(int)size.x][(int)size.y][(int)size.z];
    // (*cat)[0][0][0] = new Voxel;

    voxels = new Voxel***[(int)size.x];
    for (int x = 0; x < size.x; x++) {
        voxels[x] = new Voxel**[(int)size.y];
        for (int y = 0; y < size.y; y++) {
            voxels[x][y] = new Voxel*[(int)size.z];
            for (int z = 0; z < size.z; z++) {
                voxels[x][y][z] = new Voxel();
                float pi = 3.14;
                float norm_x = (float)x / (float)size.x;
                float norm_z = (float)z / (float)size.z;
                
                float value = (float)y / size.y;
                float wave_1 = (sin(norm_x * 2 * pi + 1) + cos(norm_z * 2 * pi * 2 + 1)) / 2.0 + 1.0;
                float wave_2 = (sin(norm_x * 5 * pi + 2.5) + cos(norm_z * 5 * pi * 2 + 4.2345)) / 2.0 + 1.0;
                float wave_3 = (sin(norm_x / 2.0) + cos(norm_z / 2.0)) / 2.0 + 1.0;
                // float wave_3 = (sin(norm_x * 2 * pi * 6 + 1.111) + cos(norm_z * 8 * pi + 2.645)) / 2.0 + 1.0;
                float threshold = (wave_1 + wave_2/3.0 + wave_3/3.0) / 3.0;
                // float threshold = (wave_1/2.0 + wave_2/2.0) / 3.0;

                glm::vec3 sand_color = {0.921568627, 0.811764706, 0.662745098};
                glm::vec3 water_color = {0.282352941, 0.541176471, 1.0};
                glm::vec3 grass_color = {0.023529412, 0.658823529, 0.443137255};
                glm::vec3 rock_color = {0.650980392, 0.650980392, 0.650980392};
                glm::vec3 snow_color = {1.0, 1.0, 1.0};

                float norm_y = (float)y / (float)size.y;

                if (norm_y <= 0.2f)
                    voxels[x][y][z]->color = water_color;
                else if (norm_y > 0.2f && norm_y <= 0.3f)
                    voxels[x][y][z]->color = sand_color;
                else if (norm_y > 0.3f && norm_y <= 0.5f)
                    voxels[x][y][z]->color = grass_color;
                else if (norm_y > 0.5f && norm_y < 0.8f)
                    voxels[x][y][z]->color = rock_color;
                else if (norm_y > 0.8f)
                    voxels[x][y][z]->color = snow_color;

                if (value > threshold)
                    voxels[x][y][z]->visible = false;
            }
        }
    }

    
        
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    VertexLayout* vertex_layout = new VertexLayout();
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
    float num_vertex_components = 9;

    for (int x = 0; x < size.x; x++) {
        for (int y = 0; y < size.y; y++) {
            for (int z = 0; z < size.z; z++) {
                if (is_free({x, y, z}))
                    continue;
                
                // glm::vec3 voxel_color = voxels[x][y][z]->color;
                glm::vec3 voxel_color = voxels[x][y][z]->color;
                
                if (is_free({x-1, y, z})) {
                    // 1 2
                    // 0 3
                    glm::vec3 normal = {-1, 0, 0};
                    
                    int first_idx = (int)vertices.size() / num_vertex_components;
                    glm::vec3 v0 = get_vertex_pos({x, y, z}, {0, 0, 0});
                    glm::vec3 v1 = get_vertex_pos({x, y, z}, {0, 1, 0});
                    glm::vec3 v2 = get_vertex_pos({x, y, z}, {0, 1, 1});
                    glm::vec3 v3 = get_vertex_pos({x, y, z}, {0, 0, 1});

                    add_vertex(vertices, v0, normal, voxel_color);
                    add_vertex(vertices, v1, normal, voxel_color);
                    add_vertex(vertices, v2, normal, voxel_color);
                    add_vertex(vertices, v3, normal, voxel_color);

                    indices.push_back(first_idx);
                    indices.push_back(first_idx+1);
                    indices.push_back(first_idx+2);

                    indices.push_back(first_idx+2);
                    indices.push_back(first_idx+3);
                    indices.push_back(first_idx);
                }

                if (is_free({x, y, z-1})) {
                    // 1 2
                    // 0 3
                    glm::vec3 normal = {0, 0, -1};
                    
                    int first_idx = (int)vertices.size() / num_vertex_components;
                    glm::vec3 v0 = get_vertex_pos({x, y, z}, {0, 0, 0});
                    glm::vec3 v1 = get_vertex_pos({x, y, z}, {0, 1, 0});
                    glm::vec3 v2 = get_vertex_pos({x, y, z}, {1, 1, 0});
                    glm::vec3 v3 = get_vertex_pos({x, y, z}, {1, 0, 0});

                    add_vertex(vertices, v0, normal, voxel_color);
                    add_vertex(vertices, v1, normal, voxel_color);
                    add_vertex(vertices, v2, normal, voxel_color);
                    add_vertex(vertices, v3, normal, voxel_color);

                    indices.push_back(first_idx);
                    indices.push_back(first_idx+1);
                    indices.push_back(first_idx+2);

                    indices.push_back(first_idx+2);
                    indices.push_back(first_idx+3);
                    indices.push_back(first_idx);
                }

                if (is_free({x, y+1, z})) {
                    // 1 2
                    // 0 3
                    glm::vec3 normal = {0, 1, 0};
                    
                    int first_idx = (int)vertices.size() / num_vertex_components;
                    glm::vec3 v0 = get_vertex_pos({x, y, z}, {0, 1, 1});
                    glm::vec3 v1 = get_vertex_pos({x, y, z}, {0, 1, 0});
                    glm::vec3 v2 = get_vertex_pos({x, y, z}, {1, 1, 0});
                    glm::vec3 v3 = get_vertex_pos({x, y, z}, {1, 1, 1});

                    add_vertex(vertices, v0, normal, voxel_color);
                    add_vertex(vertices, v1, normal, voxel_color);
                    add_vertex(vertices, v2, normal, voxel_color);
                    add_vertex(vertices, v3, normal, voxel_color);

                    indices.push_back(first_idx);
                    indices.push_back(first_idx+1);
                    indices.push_back(first_idx+2);

                    indices.push_back(first_idx+2);
                    indices.push_back(first_idx+3);
                    indices.push_back(first_idx);
                }
                if (is_free({x+1, y, z})) {
                    // 1 2
                    // 0 3
                    glm::vec3 normal = {1, 0, 0};
                    
                    int first_idx = (int)vertices.size() / num_vertex_components;
                    glm::vec3 v0 = get_vertex_pos({x, y, z}, {1, 0, 1});
                    glm::vec3 v1 = get_vertex_pos({x, y, z}, {1, 1, 1});
                    glm::vec3 v2 = get_vertex_pos({x, y, z}, {1, 1, 0});
                    glm::vec3 v3 = get_vertex_pos({x, y, z}, {1, 0, 0});

                    add_vertex(vertices, v0, normal, voxel_color);
                    add_vertex(vertices, v1, normal, voxel_color);
                    add_vertex(vertices, v2, normal, voxel_color);
                    add_vertex(vertices, v3, normal, voxel_color);

                    indices.push_back(first_idx);
                    indices.push_back(first_idx+1);
                    indices.push_back(first_idx+2);

                    indices.push_back(first_idx+2);
                    indices.push_back(first_idx+3);
                    indices.push_back(first_idx);
                }
                if (is_free({x, y, z+1})) {
                    // 1 2
                    // 0 3
                    glm::vec3 normal = {0, 0, 1};
                    
                    int first_idx = (int)vertices.size() / num_vertex_components;
                    glm::vec3 v0 = get_vertex_pos({x, y, z}, {0, 0, 1});
                    glm::vec3 v1 = get_vertex_pos({x, y, z}, {0, 1, 1});
                    glm::vec3 v2 = get_vertex_pos({x, y, z}, {1, 1, 1});
                    glm::vec3 v3 = get_vertex_pos({x, y, z}, {1, 0, 1});

                    add_vertex(vertices, v0, normal, voxel_color);
                    add_vertex(vertices, v1, normal, voxel_color);
                    add_vertex(vertices, v2, normal, voxel_color);
                    add_vertex(vertices, v3, normal, voxel_color);

                    indices.push_back(first_idx);
                    indices.push_back(first_idx+1);
                    indices.push_back(first_idx+2);

                    indices.push_back(first_idx+2);
                    indices.push_back(first_idx+3);
                    indices.push_back(first_idx);
                }
                if (is_free({x, y-1, z})) {
                    // 1 2
                    // 0 3
                    glm::vec3 normal = {0, -1, 0};
                    
                    int first_idx = (int)vertices.size() / num_vertex_components;
                    glm::vec3 v0 = get_vertex_pos({x, y, z}, {0, 0, 0});
                    glm::vec3 v1 = get_vertex_pos({x, y, z}, {0, 0, 1});
                    glm::vec3 v2 = get_vertex_pos({x, y, z}, {1, 0, 1});
                    glm::vec3 v3 = get_vertex_pos({x, y, z}, {1, 0, 0});

                    add_vertex(vertices, v0, normal, voxel_color);
                    add_vertex(vertices, v1, normal, voxel_color);
                    add_vertex(vertices, v2, normal, voxel_color);
                    add_vertex(vertices, v3, normal, voxel_color);

                    indices.push_back(first_idx);
                    indices.push_back(first_idx+1);
                    indices.push_back(first_idx+2);

                    indices.push_back(first_idx+2);
                    indices.push_back(first_idx+3);
                    indices.push_back(first_idx);
                }
            }
        }
        std::cout << x << std::endl;
    }



    this->mesh = new Mesh(vertices, indices, vertex_layout);
}

bool Chunk::is_free(glm::vec3 pos) {
    if (!in_bounds(pos))
        return true;
    if (!get_voxel(pos)->visible)
        return true;
    return false;
}

bool Chunk::in_bounds(glm::vec3 pos) {
    if (pos.x < 0 || pos.x >= size.x)
        return false;
    if (pos.y < 0 || pos.y >= size.y)
        return false;
    if (pos.z < 0 || pos.z >= size.z)
        return false;
    return true;
}

Voxel* Chunk::get_voxel(glm::vec3 pos) {
    return voxels[(int)pos.x][(int)pos.y][(int)pos.z];
}

glm::vec3 Chunk::get_vertex_pos(glm::vec3 voxel_pos, glm::vec3 corner_pos) {
    // float relative_voxel_size = 1.0f / std::max(size.x, size.y, size.z);
    float relative_voxel_size = 1.0f;
    
    glm::vec3 vertex_pos = voxel_pos * relative_voxel_size + corner_pos * relative_voxel_size;
    return vertex_pos;
}

void Chunk::add_vertex(std::vector<float>& vertices, glm::vec3 pos, glm::vec3 normal, glm::vec3 color) {
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

void Chunk::draw(RenderState state) {
    state.transform *= get_model_matrix();
    mesh->draw(state);
}