#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>
#include <cstring>

class EBO{
public:
    GLuint id = 0;
    size_t size_bytes = 0;
    size_t num_indices = 0;
    size_t capacity_bytes = 0;

    EBO(const void* indices, size_t size_bytes);
    ~EBO();
    void update_mapped(const void* data, size_t new_size_bytes, GLenum usage);
    void bind() const;
    static void unbind();
};