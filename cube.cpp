#include "cube.h"

Mesh* Cube::mesh = nullptr;

Cube::Cube() {
    if (Cube::mesh)
        return;

    std::vector<float> vertices_vec = {
        // positions            // normals

        // Front (+Z)
        -1, -1,  1,   0,  0,  1,
        1, -1,  1,   0,  0,  1,
        1,  1,  1,   0,  0,  1,
        -1,  1,  1,   0,  0,  1,

        // Back (-Z)
        1, -1, -1,   0,  0, -1,
        -1, -1, -1,   0,  0, -1,
        -1,  1, -1,   0,  0, -1,
        1,  1, -1,   0,  0, -1,

        // Left (-X)
        -1, -1, -1,  -1,  0,  0,
        -1, -1,  1,  -1,  0,  0,
        -1,  1,  1,  -1,  0,  0,
        -1,  1, -1,  -1,  0,  0,

        // Right (+X)
        1, -1,  1,   1,  0,  0,
        1, -1, -1,   1,  0,  0,
        1,  1, -1,   1,  0,  0,
        1,  1,  1,   1,  0,  0,

        // Top (+Y)
        -1,  1,  1,   0,  1,  0,
        1,  1,  1,   0,  1,  0,
        1,  1, -1,   0,  1,  0,
        -1,  1, -1,   0,  1,  0,

        // Bottom (-Y)
        -1, -1, -1,   0, -1,  0,
        1, -1, -1,   0, -1,  0,
        1, -1,  1,   0, -1,  0,
        -1, -1,  1,   0, -1,  0,
    };

    std::vector<unsigned int> indices_vec = {
        0,  1,  2,  2,  3,  0,   // front
        4,  5,  6,  6,  7,  4,   // back
        8,  9, 10, 10, 11,  8,   // left
        12, 13, 14, 14, 15, 12,   // right
        16, 17, 18, 18, 19, 16,   // top
        20, 21, 22, 22, 23, 20    // bottom
    };


    VertexLayout* vertex_layout = new VertexLayout();
    vertex_layout->add({
        0, 3, GL_FLOAT, GL_FALSE,
        6 * sizeof(float),
        0
    });
    vertex_layout->add({
        1, 3, GL_FLOAT, GL_FALSE,
        6 * sizeof(float),
        3 * sizeof(float)
    });

    Cube::mesh = new Mesh(vertices_vec, indices_vec, vertex_layout);
}

void Cube::draw(RenderState state) {
    state.transform *= get_model_matrix();
    mesh->draw(state);
}