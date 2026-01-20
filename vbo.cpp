#include "vbo.h"


VBO::VBO(const void* data, size_t size_bytes, GLenum usage) {
    if (data == nullptr || size_bytes == 0) {
        throw std::invalid_argument("VBO: empty data");
    }
    glGenBuffers(1, &id);
    if (id == 0)
        throw std::runtime_error("VBO: glGenBuffers failed (no GL context?)");
    this->bind();
    glBufferData(GL_ARRAY_BUFFER, size_bytes, data, usage);
    this->unbind();

    this->size_bytes = size_bytes;
}

VBO::~VBO() {
    if (id != 0)
        glDeleteBuffers(1, &id);
}

void VBO::bind() const {
    glBindBuffer(GL_ARRAY_BUFFER, id);
}

void VBO::unbind() {
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}