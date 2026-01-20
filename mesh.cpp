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

void Mesh::set_instance_transforms(const std::vector<glm::mat4>& transforms) {
    instance_transforms = transforms;

    if (!instance_vbo)
        instance_vbo = new VBO(transforms.data(), transforms.size() * sizeof(glm::mat4), GL_DYNAMIC_DRAW);
    else {
        instance_vbo->bind();
        glBufferData(GL_ARRAY_BUFFER, transforms.size() * sizeof(glm::mat4), transforms.data(), GL_DYNAMIC_DRAW);
    }

    vao->bind();

    // Each mat4 = 4 vec4s
    for (int i = 0; i < 4; i++) {
        GLuint attrib_index = 2 + i; // 2,3,4,5 are for instance matrix
        glEnableVertexAttribArray(attrib_index);
        glVertexAttribPointer(attrib_index, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * sizeof(glm::vec4)));
        glVertexAttribDivisor(attrib_index, 1); // advance per instance
    }

    vao->unbind();
}


void Mesh::draw_instanced(RenderState state, Program* program) {
    if (!vao || instance_transforms.empty())
        return;
    
    program->use();
    if (state.camera)
        program->set_vec3("uViewPos", state.camera->position);

    vao->bind();
    glDrawElementsInstanced(GL_TRIANGLES, ebo->num_indices, GL_UNSIGNED_INT, 0, instance_transforms.size());
    vao->unbind();
}