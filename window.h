#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <chrono>

#include "drawable.h"
#include "camera.h"

class Window{
public:
    GLFWwindow* window;
    int width;
    int height;

    Window(int width, int height, std::string title);
    ~Window();
    void swap_buffers();
    bool is_open();
    float get_aspect_ratio();
    float get_fbuffer_aspect_ratio();
    void clear_color(const glm::vec4& color);
    void draw(Drawable* drawable, Camera* camera, Program* program = nullptr);
};