#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <chrono>

#include "shader.h"

class FragmentShader : public Shader {
public:
    FragmentShader() = default;
    FragmentShader(std::string path);

    FragmentShader(const FragmentShader&) = delete;
    FragmentShader& operator=(const FragmentShader&) = delete;
    FragmentShader(FragmentShader&& o) noexcept;
    FragmentShader& operator=(FragmentShader&& o) noexcept;
};