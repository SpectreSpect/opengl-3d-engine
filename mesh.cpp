#include "mesh.h"

Mesh::Mesh(std::vector<float> vertices, std::vector<unsigned int> indices, VertexLayout* vertex_layout) {

    // Mesh::Mesh(vertices.data(), vertices.size() * sizeof(float), indices.data(), indices.size() * sizeof(unsigned int), vertex_layout);
    vao = new VAO();
    vbo = new VBO(vertices.data(), vertices.size() * sizeof(float));
    ebo = new EBO(indices.data(), indices.size() * sizeof(unsigned int));
    this->vertex_layout = vertex_layout;

    vao->setup(vbo, ebo, vertex_layout);
}

// Mesh::Mesh(const void* vertex_data, size_t vertex_data_size, const void* index_data, size_t index_data_size, VertexLayout* vertex_layout) {
//     vao = new VAO();
//     vbo = new VBO(vertex_data, vertex_data_size);
//     ebo = new EBO(index_data, index_data_size);
//     this->vertex_layout = vertex_layout;

//     vao->setup(vbo, ebo, vertex_layout);
// }

void Mesh::update(const std::vector<float> vertices, const std::vector<unsigned int>& indices, GLenum usage) {
    if (vbo)
        vbo->update_mapped(vertices.data(), vertices.size() * sizeof(float), usage);
    
    if (ebo)
        ebo->update_mapped(indices.data(), indices.size() * sizeof(unsigned int), usage);
}

void Mesh::update(const void* vertex_data, size_t vertex_data_size, const void* index_data, size_t index_data_size, GLenum usage) {
    if (vbo)
        vbo->update_mapped(vertex_data, vertex_data_size, usage);
    
    if (ebo)
        ebo->update_mapped(index_data, index_data_size, usage);
}

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
