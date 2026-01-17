#include "vao.h"

VAO::VAO() {
    glGenVertexArrays(1, &this->id);
    if (this->id == 0)
        throw std::runtime_error("Failed to create VAO");
    
}

VAO::~VAO() {
    glDeleteVertexArrays(1, &id);
}



void VAO::setup(VBO* vbo, EBO* ebo, VertexLayout* vertex_layout) {
    this->bind();

    vbo->bind();
    ebo->bind();
    vertex_layout->apply();

    this->unbind();
}

void VAO::bind() const {
    glBindVertexArray(id);
}

void VAO::unbind() {
    glBindVertexArray(0);
}
    // GLint posAttribIndex = 0;
    // GLint posSize = 3;
    // GLsizei stride = 6 * sizeof(float);
    // const void* posOffset = (void*)0;
    // GLint colorAttribIndex = 1; // ??
    // GLint colorSize = 3;
    // const void* colorOffset =(void*)(3 * sizeof(float));

    // glVertexAttribPointer(posAttribIndex, posSize, GL_FLOAT, GL_FALSE, stride, posOffset);

    // if (colorAttribIndex >= 0) {
    //     glEnableVertexAttribArray(colorAttribIndex);
    //     glVertexAttribPointer(colorAttribIndex, colorSize, GL_FLOAT, GL_FALSE, stride, colorOffset);
    // }