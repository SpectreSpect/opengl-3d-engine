#include "mesh.h"

Mesh::Mesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, const VertexLayout& vertex_layout)
    :   vbo(BufferObject(vertices.size() * sizeof(float), GL_STATIC_DRAW, vertices.data())),
        ebo(BufferObject(indices.size() * sizeof(unsigned int), GL_STATIC_DRAW, indices.data())),
        vertex_layout(vertex_layout)
{
    vao.setup(vbo, ebo, vertex_layout);
}

void Mesh::update(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, GLenum usage) {
    vbo.update_subdata(0, vertices.size() * sizeof(float), vertices.data());
    ebo.update_subdata(0, indices.size() * sizeof(unsigned int), indices.data());
}

void Mesh::update(const void* vertex_data, size_t vertex_data_size, const void* index_data, size_t index_data_size, GLenum usage) {
    vbo.update_subdata(0, vertex_data_size, vertex_data);
    ebo.update_subdata(0, index_data_size, index_data);
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

    vao.bind();
    glDrawElements(GL_TRIANGLES, ebo.size_bytes() / sizeof(uint32_t), GL_UNSIGNED_INT, 0);
    vao.unbind();
}
