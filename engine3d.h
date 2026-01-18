#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <chrono>

// #include "window.h"
#include "camera.h"
#include "vertex_shader.h"
#include "fragment_shader.h"
#include "program.h"

#include "mesh_manager.h"
#include "material_manager.h"

class Window;


class Engine3D{
public:
    Engine3D();
    ~Engine3D();

    MeshManager* mesh_manager;
    MaterialManager* material_manager;

    std::string default_vertex_shader_path = "../shaders/deafult_vertex.glsl";
    std::string default_fragment_shader_path = "../shaders/deafult_fragment.glsl";

    VertexShader* default_vertex_shader;
    FragmentShader* default_fragment_shader;
    Program* default_program;

    // Camera* camera;

    int init();
    int init_glew();
    void set_window(Window* window);
    // void set_camera(Camera* camera);
    static void enable_depth_test();
    static void poll_events();
};