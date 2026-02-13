#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <chrono>

#include "shader.h"

class VertexShader : public Shader {
public:
    VertexShader() = default;
    VertexShader(std::string path);
    
    VertexShader(const VertexShader&) = delete;
    VertexShader& operator=(const VertexShader&) = delete;
    VertexShader(VertexShader&& o) noexcept;
    VertexShader& operator=(VertexShader&& o) noexcept;
};