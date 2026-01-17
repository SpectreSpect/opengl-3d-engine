#include "ebo.h"


EBO::EBO(const void* indices, size_t num_indices) {
    if (!indices || num_indices == 0) 
        throw std::invalid_argument("EBO: empty data");
    
        glGenBuffers(1, &id);
    if (id == 0) 
        throw std::runtime_error("EBO: glGenBuffers failed (no GL context?)");

    this->bind();
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, num_indices, indices, GL_STATIC_DRAW);
    this->unbind();

    this->num_indices = num_indices;
}

EBO::~EBO() {
    if (id != 0)
        glDeleteBuffers(1, &id);
}

void EBO::bind() const {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
}

void EBO::unbind() { 
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); 
}