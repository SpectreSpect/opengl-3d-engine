#include "ebo.h"


EBO::EBO(const void* indices, size_t size_bytes) {
    if (!indices || size_bytes == 0) 
        throw std::invalid_argument("EBO: empty data");
    
        glGenBuffers(1, &id);
    if (id == 0) 
        throw std::runtime_error("EBO: glGenBuffers failed (no GL context?)");

    this->bind();
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, size_bytes, indices, GL_STATIC_DRAW);
    this->unbind();

    this->size_bytes = size_bytes;
    this->num_indices = size_bytes / sizeof(unsigned int);
}

EBO::~EBO() {
    if (id != 0)
        glDeleteBuffers(1, &id);
}

void EBO::update_mapped(const void* data, size_t new_size_bytes, GLenum usage) {
    if (!data || new_size_bytes == 0)
        return;
    
    assert(new_size_bytes % sizeof(unsigned int) == 0);

    bind();

    if (new_size_bytes > capacity_bytes) {
        size_t new_capacity = std::max(new_size_bytes, capacity_bytes ? capacity_bytes * 2 : new_size_bytes);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, new_capacity, nullptr, usage); // orphan
        capacity_bytes = new_capacity;
    }

    void* ptr = glMapBufferRange(
        GL_ELEMENT_ARRAY_BUFFER,
        0,
        new_size_bytes,
        GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT
    );

    if (ptr) {
        memcpy(ptr, data, new_size_bytes);
        glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);   
    } else {
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, new_size_bytes, data);
    }

    this->size_bytes = new_size_bytes;
    this->num_indices = new_size_bytes / sizeof(unsigned int);
}

void EBO::bind() const {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
}

void EBO::unbind() { 
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); 
}