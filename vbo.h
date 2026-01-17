#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>

class VBO{
public:
    int size_bytes;
    GLuint id;
    VBO(const void* data, size_t size_bytes);
    ~VBO();
    void bind() const;
    static void unbind();
};
