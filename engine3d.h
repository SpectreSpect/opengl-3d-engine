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

class Window;



class Engine3D{
public:
    Engine3D();
    ~Engine3D();

    MeshManager* mesh_manager;
    MaterialManager* material_manager;
    ShaderManager* shader_manager;


    // VfProgram* default_program;
    VfProgram* default_line_program;
    VfProgram* default_point_program;

    std::string default_vertex_shader_path = (executable_dir() / "shaders" / "deafult_vertex.glsl").string();
    std::string default_fragment_shader_path = (executable_dir() / "shaders" / "deafult_fragment.glsl").string();

    VertexShader* default_vertex_shader;
    FragmentShader* default_fragment_shader;
    VfProgram* default_program;

    size_t max_num_light_sources = 10000;
    std::vector<LightSource> light_sources;
    std::unordered_set<size_t> dirty_lights;
    SSBO cluster_aabbs_ssbo;
    SSBO light_source_ssbo;
    SSBO num_lights_in_clusters_ssbo;
    SSBO lights_in_clusters_ssbo;
    size_t max_lights_per_cluster = 1500;

    std::vector<std::vector<size_t>> lights_in_clusters;
    // glm::vec3 num_clusters{10, 10, 10};
    glm::vec3 num_clusters{25, 25, 25};

    


    // std::string default_vertex_shader_path = (executable_dir() / "shaders" / "deafult_vertex.glsl").string();
    // std::string default_fragment_shader_path = (executable_dir() / "shaders" / "deafult_fragment.glsl").string();

    // VertexShader* default_vertex_shader;
    // FragmentShader* default_fragment_shader;
    // VfProgram* default_program;


    // std::string default_line_vertex_shader_path = (executable_dir() / "shaders" / "line_vs.glsl").string();
    // std::string default_line_fragment_shader_path = (executable_dir() / "shaders" / "line_fs.glsl").string();

    // VertexShader* default_line_vertex_shader;
    // FragmentShader* default_line_fragment_shader;
    // Program* default_line_program;


    // std::string default_point_vertex_shader_path = (executable_dir() / "shaders" / "point_vs.glsl").string();
    // std::string default_point_fragment_shader_path = (executable_dir() / "shaders" / "point_fs.glsl").string();

    // VertexShader* default_point_vertex_shader;
    // FragmentShader* default_point_fragment_shader;
    // Program* default_point_program;


    // Camera* camera;

    int init();
    int init_glew();
    void set_window(Window* window);
    
    void update_lights_in_clusters();
    void set_light_source(size_t id, LightSource light_source);
    void update_light_sources();
    bool sphereIntersectsAABB_ViewSpace(const glm::vec3 &centerVS, float radius, const AABB &aabb);
    void update_clusters(const std::vector<AABB> &clusters, const glm::mat4& view_matrix);
    void update_light_indices_for_clusters(ComputeProgram& light_indices_for_clusters_program, const Camera& camera);
    void set_cluster_aabbs(std::vector<AABB>& aabbs);

    // void set_camera(Camera* camera);
    static void enable_depth_test();
    static void poll_events();
};