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
    GLuint id;
    // VBO* vbo;
    // EBO* ebo;

    VAO();
    ~VAO();
    void setup(VBO* vbo, EBO* ebo, VertexLayout* vertex_layout);

    void bind() const;
    static void unbind();
};