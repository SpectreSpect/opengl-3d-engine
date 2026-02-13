#include "vao.h"

VAO::~VAO() {
    if (id != 0)
        glDeleteVertexArrays(1, &id);
}

VAO::VAO(VAO&& o) noexcept {
    id = o.id;
    o.id = 0;
}

VAO& VAO::operator=(VAO&& o) noexcept {
    if (this != &o) {
        if (id != 0)
            glDeleteVertexArrays(1, &id);

        id = o.id;
        o.id = 0;
    }
    
    return *this;
}

VAO& VAO::init_vao() & {
    glGenVertexArrays(1, &this->id);
    if (this->id == 0)
        throw std::runtime_error("Failed to create VAO");
    
    return *this;
}

VAO&& VAO::init_vao() && {
    static_cast<VAO&>(*this).init_vao();
    return std::move(*this);
}


void VAO::setup(const VBO& vbo, const EBO& ebo, const VertexLayout& vertex_layout) {
    this->bind();

    vbo.bind();
    ebo.bind();
    vertex_layout.apply();

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