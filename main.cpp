// main.cpp
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <chrono>
#include <cmath>

#include "engine3d.h"

#include "vao.h"
#include "vbo.h"
#include "ebo.h"
#include "vertex_layout.h"
#include "program.h"
#include "camera.h"
#include "mesh.h"
#include "cube.h"
#include "window.h"
#include "fps_camera_controller.h"
#include "voxel_engine/chunk.h"
#include "voxel_engine/voxel_grid.h"
#include "imgui_layer.h"
#include "voxel_rastorizator.h"
#include "ui_elements/triangle_controller.h"
#include "triangle.h"
#include "a_star/a_star.h"
#include "line.h"
#include "a_star/nonholonomic_a_star.h"
#include "a_star/reeds_shepp.h"

#include <cstdint>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include "point.h"
#include "point_cloud/point_cloud_frame.h"
#include "point_cloud/point_cloud_video.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>
#include <cmath>
#include <cstdint>
#include "math_utils.h"
#include "light_source.h"


// float clear_col[4] = {0.776470588f, 0.988235294f, 1.0f, 1.0f};
// float clear_col[4] = {0.1f, 0.1f, 0.1f, 1.0f};
float clear_col[4] = {0.0f, 0.0f, 0.0f, 1.0f};


void set_light_position(SSBO& light_source_ssbo, size_t index, const glm::vec4 &pos) {
    size_t offset = index * sizeof(LightSource);
    light_source_ssbo.update_subdata(offset, &pos, sizeof(pos));
}


int main() {
    Engine3D engine = Engine3D();
    Window window = Window(&engine, 1280, 720, "3D visualization");
    engine.set_window(&window);
    ui::init(window.window);
    
    // ShaderManager shader_manager = ShaderManager(executable_dir_str());

    Camera camera;
    window.set_camera(&camera);

    FPSCameraController camera_controller = FPSCameraController(&camera);
    camera_controller.speed = 50;

    VoxelGrid* voxel_grid = new VoxelGrid({16, 16, 16}, 1.0f, {24, 6, 24});
    voxel_grid->update(&window, &camera);

    glm::vec3 origin = glm::vec3(0.0f, 10.0f, 0.0f);

    std::vector<Line*> point_cloud_video_lines;
    std::vector<Point*> point_cloud_video_points;
    std::vector<Mesh*> lidar_meshes;

    PointCloudFrame point_cloud_frame = PointCloudFrame("/home/spectre/TEMP_lidar_output_mesh/recording/frame_000000.bin");
    size_t rings_count = 16;
    size_t ring_size = point_cloud_frame.point_cloud.size() / rings_count;

    // double t0 = math_utils::ms_now();
    // Mesh point_cloud_mesh = point_cloud_frame.point_cloud.generate_mesh(1.5);
    
    Mesh point_cloud_mesh = point_cloud_frame.point_cloud.generate_mesh_gpu(rings_count, ring_size, 1.5);
    // double t1 = math_utils::ms_now();

    // std::cout << t1 - t0 << " ms" << std::endl;

    // PointCloudVideo point_cloud_video = PointCloudVideo();
    // point_cloud_video.load_from_file("/home/spectre/TEMP_lidar_output_mesh/recording/index.csv");

    // size_t max_num_light_sources = 20;
    // std::vector<LightSource> light_sources(max_num_light_sources);

    // light_sources[0] = {glm::vec4(-10.0f, 18.0f, 10.0f, 20.0f), glm::vec4(0.9f, 0.9f, 1.0f, 1.0f)};
    // light_sources[1] = {glm::vec4(-30.0f, 18.0f, 10.0f, 20.0f), glm::vec4(0.9f, 0.9f, 1.0f, 1.0f)};
    // light_sources[2] = {glm::vec4(-50.0f, 18.0f, 10.0f, 20.0f), glm::vec4(0.9f, 0.9f, 1.0f, 1.0f)};


    // light_sources[0] = {glm::vec4(-10.0f, 5.0f, 10.0f, 20.0f), glm::vec4(0.9843f, 0.7412f, 0.3980f, 1.0f)};
    // light_sources[1] = {glm::vec4(-30.0f, 5.0f, 10.0f, 20.0f), glm::vec4(0.9843f, 0.7412f, 0.3980f, 1.0f)};
    // light_sources[2] = {glm::vec4(-50.0f, 5.0f, 10.0f, 20.0f), glm::vec4(0.9843f, 0.7412f, 0.3980f, 1.0f)};

    // SSBO light_source_ssbo(sizeof(LightSource) * max_num_light_sources, GL_DYNAMIC_DRAW, light_sources.data());

    // glm::vec4 light_pos = light_sources[0].position;


    engine.set_light_source(0, {glm::vec4(-10.0f, 18.0f, 10.0f, 20.0f), glm::vec4(0.9f, 0.9f, 1.0f, 1.0f)});
    engine.set_light_source(1, {glm::vec4(-30.0f, 18.0f, 10.0f, 20.0f), glm::vec4(0.9f, 0.9f, 1.0f, 1.0f)});
    engine.set_light_source(2, {glm::vec4(-50.0f, 18.0f, 10.0f, 20.0f), glm::vec4(0.9f, 0.9f, 1.0f, 1.0f)});
    


    // std::vector<LightSource> read_light_sources(3);
    // light_source_ssbo.read_subdata(0, read_light_sources.data(), read_light_sources.size() * sizeof(LightSource));

    // std::cout << light_sources[0].color.r << std::endl;

    
    float rel_thresh = 1.5f;
    float last_frame = 0.0f;
    float timer = 0.0f;
    // window->disable_cursor();
    while(window.is_open()) {
        engine.update_light_sources();

        float currentFrame = (float)glfwGetTime();
        float delta_time = currentFrame - last_frame;
        timer += delta_time;
        last_frame = currentFrame;   

        ui::begin_frame();
        ui::update_mouse_mode(&window);

        camera_controller.update(&window, delta_time);

        window.clear_color({clear_col[0], clear_col[1], clear_col[2], clear_col[3]});


        // light_source_ssbo.bind_base(5);
        // engine.default_program->set_uint("num_light_sources", light_sources.size());

        voxel_grid->update(&window, &camera);
        window.draw(voxel_grid, &camera);
        
        ImGui::Begin("Debug");

        glm::vec3 p = camera.position; // or any vec3

        ImGui::TextColored(ImVec4(1,0.5,0.5,1), "x: %.3f", p.x);
        ImGui::TextColored(ImVec4(0.5,1,0.5,1), "y: %.3f", p.y);
        ImGui::TextColored(ImVec4(0.5,0.5,1,1), "z: %.3f", p.z);

        ImGui::SliderFloat("Relative threshold", &rel_thresh, 0.0f, 10.0f);

        ImGui::End();

        ui::end_frame();
    
        // point_cloud_video.update(delta_time);
        // window.draw(&point_cloud_video, &camera);

        // window.draw(&point_cloud_frame, &camera);
        
        // window.draw(&point_cloud_mesh, &camera);
        

        window.swap_buffers();
        engine.poll_events();
    }
    
    ui::shutdown();
}
