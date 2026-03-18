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
    VertexShader(std::string path);
};