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
#include <vector>
#include <filesystem>

#include "glsl_preprocessor.h"

class Shader {
public:
    GLuint id;

    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& o) noexcept { id = o.id; o.id = 0; }
    Shader& operator=(Shader&& o) noexcept {
        if (this != &o) {
            if (id) glDeleteShader(id);
            id = o.id;
            o.id = 0;
        }
        return *this;
    }
    void print_shader_log(const char* name = "NO NAME");

    static std::string load_text_file(
        const std::filesystem::path& path, 
        const std::vector<std::filesystem::path>& include_directories = std::vector<std::filesystem::path>()
    );
    static GLuint compile_shader(GLenum type, const char* src, const std::filesystem::path* shader_path = nullptr);
};