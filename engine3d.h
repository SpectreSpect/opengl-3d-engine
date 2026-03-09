#pragma once

#include <filesystem>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>
#include <string>
#include <chrono>

// #include "window.h"
#include "camera.h"
#include "vertex_shader.h"
#include "fragment_shader.h"
#include "vf_program.h"

#include "mesh_manager.h"
#include "material_manager.h"
#include "path_utils.h"
#include "shader_manager.h"
#include "path_utils.h"
#include "light_source.h"
#include "ssbo.h"
#include <unordered_set>
#include "math_utils.h"
#include "compute_program.h"
#include "lighting_system.h"

class Window;



class Engine3D{
public:
    Engine3D();
    ~Engine3D();

    MeshManager* mesh_manager;
    MaterialManager* material_manager;
    ShaderManager* shader_manager;
    LightingSystem lighting_system;

    VfProgram* default_line_program;
    VfProgram* default_point_program;
    VfProgram* default_circle_program;
    VfProgram* skybox_program;
    VfProgram* equirect_to_cubemap_program;
    VfProgram* irradiance_program;
    VfProgram* prefilter_program;
    VfProgram* brdf_lut_program;
    
    
    

    // std::string default_vertex_shader_path = (executable_dir() / "shaders" / "deafult_vertex.glsl").string();
    // std::string default_fragment_shader_path = (executable_dir() / "shaders" / "deafult_fragment.glsl").string();

    // VertexShader* default_vertex_shader;
    // FragmentShader* default_fragment_shader;
    VfProgram* default_program;

    int init();
    int init_glew();
    void set_window(Window* window);
    void update_lighting_system(const Camera& camera, const Window& window);

    static void enable_depth_test();
    static void poll_events();
};