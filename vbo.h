#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>
#include <cstring>

class VBO{
public:
    size_t size_bytes = 0;
    size_t capacity_bytes = 0;
    GLuint id;
    VBO(const void* data, size_t size_bytes);
    ~VBO();
    // void update(const void* data, size_t new_size_bytes, GLenum usage = GL_DYNAMIC_DRAW);
    void update_mapped(const void* data, size_t new_size_bytes, GLenum usage = GL_DYNAMIC_DRAW);
    void bind() const;
    static void unbind();
};
