#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>

class EBO{
public:
    GLuint id{0};
    size_t num_indices{0};

    const void* indices;

    EBO(const void* indices, size_t num_indices);
    ~EBO();

    void bind() const;
    static void unbind();
};