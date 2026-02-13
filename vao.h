#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "vbo.h"
#include "ebo.h"
#include "vertex_layout.h"

class VAO {
public:
    GLuint id = 0;
    // VBO* vbo;
    // EBO* ebo;

    VAO() = default;
    ~VAO();

    VAO(const VBO&) = delete;
    VAO& operator=(const VAO&) = delete;
    VAO(VAO&& o) noexcept;
    VAO& operator=(VAO&& o) noexcept;

    VAO& init_vao() &;
    VAO&& init_vao() &&;
    void setup(const VBO& vbo, const EBO& ebo, const VertexLayout& vertex_layout);

    void bind() const;
    static void unbind();
};