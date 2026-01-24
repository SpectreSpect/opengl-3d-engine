#include "cube.h"

Cube::Cube(glm::vec3 position, glm::vec3 scale, glm::vec3 color) {
    this->position = position;
    this->scale = scale;
    this->_color = color;

    MeshData mesh_data = create_mesh_data(color);

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

    mesh = new Mesh(mesh_data.vertices, mesh_data.indices, vertex_layout);
}

MeshData Cube::create_mesh_data(glm::vec3 color) const {
    std::vector<float> vertices = {
        // positions            // normals

        // Front (+Z)
        -1, -1,  1,   0,  0,  1, color.x, color.y, color.z,
        1, -1,  1,   0,  0,  1, color.x, color.y, color.z,
        1,  1,  1,   0,  0,  1, color.x, color.y, color.z,
        -1,  1,  1,   0,  0,  1, color.x, color.y, color.z,

        // Back (-Z)
        1, -1, -1,   0,  0, -1, color.x, color.y, color.z,
        -1, -1, -1,   0,  0, -1, color.x, color.y, color.z,
        -1,  1, -1,   0,  0, -1, color.x, color.y, color.z,
        1,  1, -1,   0,  0, -1, color.x, color.y, color.z,

        // Left (-X)
        -1, -1, -1,  -1,  0,  0, color.x, color.y, color.z,
        -1, -1,  1,  -1,  0,  0, color.x, color.y, color.z,
        -1,  1,  1,  -1,  0,  0, color.x, color.y, color.z,
        -1,  1, -1,  -1,  0,  0, color.x, color.y, color.z,

        // Right (+X)
        1, -1,  1,   1,  0,  0, color.x, color.y, color.z,
        1, -1, -1,   1,  0,  0, color.x, color.y, color.z,
        1,  1, -1,   1,  0,  0, color.x, color.y, color.z,
        1,  1,  1,   1,  0,  0, color.x, color.y, color.z,
        // Top (+Y)
        -1,  1,  1,   0,  1,  0, color.x, color.y, color.z,
        1,  1,  1,   0,  1,  0, color.x, color.y, color.z,
        1,  1, -1,   0,  1,  0, color.x, color.y, color.z,
        -1,  1, -1,   0,  1,  0, color.x, color.y, color.z,

        // Bottom (-Y)
        -1, -1, -1,   0, -1,  0, color.x, color.y, color.z,
        1, -1, -1,   0, -1,  0, color.x, color.y, color.z,
        1, -1,  1,   0, -1,  0, color.x, color.y, color.z,
        -1, -1,  1,   0, -1,  0, color.x, color.y, color.z,
    };

    std::vector<unsigned int> indices = {
        0,  1,  2,  2,  3,  0,   // front
        4,  5,  6,  6,  7,  4,   // back
        8,  9, 10, 10, 11,  8,   // left
        12, 13, 14, 14, 15, 12,   // right
        16, 17, 18, 18, 19, 16,   // top
        20, 21, 22, 22, 23, 20    // bottom
    };
    
    MeshData mesh_data;
    mesh_data.vertices = std::move(vertices);
    mesh_data.indices = std::move(indices);

    return mesh_data;
}

void Cube::set_color(glm::vec3& color) {
    _color = color;
    MeshData mesh_data = create_mesh_data(color);
    mesh->update(mesh_data.vertices, mesh_data.indices);
}

void Cube::draw(RenderState state) {
    state.transform *= get_model_matrix();
    mesh->draw(state);
}