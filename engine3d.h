#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <chrono>

#include "window.h"
#include "camera.h"

class Engine3D{
public:
    Engine3D();
    ~Engine3D();

    Camera* camera;

    int init();
    int init_glew();
    void make_context_current(Window* window);
    void set_camera(Camera* camera);
    static void enable_depth_test();
    static void poll_events();
};