#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <chrono>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

class Shader {
public:
    GLuint id;

    Shader() = default;
    static std::string load_text_file(const std::string& path);
    static GLuint compile_shader(GLenum type, const char* src);
};