#include "mesh.h"

Mesh::Mesh(std::vector<float> vertices, std::vector<unsigned int> indices, VertexLayout* vertex_layout) {
    vao = new VAO();
    vbo = new VBO(vertices.data(), vertices.size() * sizeof(float));
    ebo = new EBO(indices.data(), indices.size() * sizeof(unsigned int));
    this->vertex_layout = vertex_layout;

    vao->setup(vbo, ebo, vertex_layout);
}

// Mesh::Mesh(float* vertices, unsigned int* indices, int vertices_size, int indices_size, Program* shader, VertexLayout* vertex_layout) {
//     vao = new VAO();
//     vbo = new VBO(vertices, vertices_size);
//     ebo = new EBO(indices, indices_size);
//     this->vertex_layout = vertex_layout;

//     vao->setup(vbo, ebo, vertex_layout);
// }


void Mesh::draw(RenderState state) {
    glm::mat4 model = get_model_matrix();
    glm::mat4 world = state.transform * model;
    glm::mat4 mvp = state.vp * world;


    Program* prog = state.program;
    prog->use();
    prog->set_mat4("uModel", world);
    prog->set_mat4("uMVP", mvp);

    if (state.camera) {
        prog->set_vec3("uViewPos", state.camera->position);
    }

    vao->bind();
    glDrawElements(GL_TRIANGLES, ebo->num_indices, GL_UNSIGNED_INT, 0);
    vao->unbind();
}
