#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <chrono>

struct VertexAttribute {
    std::string name;
    GLuint index;
    GLint size;
    GLenum type = GL_FLOAT;
    GLboolean normalized = GL_FALSE;
    GLsizei stride;
    size_t offset;
    GLuint divisor = 0;   // for instancing
    std::vector<float> default_value;
};