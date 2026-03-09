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
#include "circle_cloud.h"
#include "texture.h"
#include "cubemap.h"
#include "skybox.h"
#include "framebuffer.h"
#include "sphere.h"
#include "cube.h"
#include "quad.h"
#include "texture_manager.h"
#include "pbr_skybox.h"

// float clear_col[4] = {0.15f, 0.15f, 0.18f, 1.0f};
float clear_col[4] = {0, 0, 0, 1.0f};

void set_light_position(SSBO& light_source_ssbo, size_t index, const glm::vec4 &pos) {
    size_t offset = index * sizeof(LightSource);
    light_source_ssbo.update_subdata(offset, &pos, sizeof(pos));
}


int main() {
    Engine3D engine = Engine3D();
    Window window = Window(&engine, 1280, 720, "3D visualization");
    engine.set_window(&window);
    ui::init(window.window);

    Camera camera;
    window.set_camera(&camera);
    FPSCameraController camera_controller = FPSCameraController(&camera);
    
    camera_controller.speed = 50;

    VoxelGrid* voxel_grid = new VoxelGrid({16, 16, 16}, 1.0f, {24, 10, 24});
    voxel_grid->update(&window, &camera);
    sleep(1);

    glm::vec3 origin = glm::vec3(0.0f, 10.0f, 0.0f);

    std::vector<Line*> point_cloud_video_lines;
    std::vector<Point*> point_cloud_video_points;
    std::vector<Mesh*> lidar_meshes;

    PointCloudFrame point_cloud_frame = PointCloudFrame("/home/spectre/TEMP_lidar_output_mesh/recording/frame_000000.bin");
    size_t rings_count = 16;
    size_t ring_size = point_cloud_frame.point_cloud.size() / rings_count;
    
    Mesh point_cloud_mesh = point_cloud_frame.point_cloud.generate_mesh_gpu(rings_count, ring_size, 1.5);

    CirlceInstance circle_instance;
    // circle_instance.color = glm::vec4(1, 1, 1, 1);
    circle_instance.color = glm::vec4(0.8, 0.8, 0.2, 1);

    std::vector<CirlceInstance> instances;

    int row_size = 8;
    for (int x = 0; x < row_size; x++)
        for (int z = 0; z < row_size; z++) {
            circle_instance.position_radius.x = (x + 0.5) * 2;
            circle_instance.position_radius.z = (z + 0.5) * 2;

            circle_instance.color = glm::vec4((float)x / (float)row_size, 0, (float)z / (float)row_size, 1);

            instances.push_back(circle_instance);
        }
    
    CircleCloud circle_cloud = CircleCloud();
    circle_cloud.set_instances(instances);

    LightSource light_source;
    light_source.position = glm::vec4(-15, 2, 2, 8);
    light_source.color = glm::vec4(1, 0, 0, 1);
    engine.lighting_system.set_light_source(0, light_source);

    light_source.position = glm::vec4(-15, 2, -2, 8);
    light_source.color = glm::vec4(0, 1, 0, 1);
    engine.lighting_system.set_light_source(1, light_source);

    light_source.position = glm::vec4(-13, 2, 0, 8);
    light_source.color = glm::vec4(0, 0, 1, 1);
    engine.lighting_system.set_light_source(2, light_source);

    light_source.position = glm::vec4(-17, 2, 0, 8);
    light_source.color = glm::vec4(1, 1, 1, 1);
    engine.lighting_system.set_light_source(3, light_source);

    Sphere sphere = Sphere();
    sphere.mesh->position.x = -14;
    sphere.mesh->position.y = 1;

    PBRSkybox skybox = PBRSkybox(engine.texture_manager->st_peters_square_night_4k);
        
    float rel_thresh = 1.5f;
    float last_frame = 0.0f;
    float timer = 0.0f;
    while(window.is_open()) {
        float currentFrame = (float)glfwGetTime();
        float delta_time = currentFrame - last_frame;
        timer += delta_time;
        last_frame = currentFrame;   
        float fps = 1.0f / delta_time;
        
        camera_controller.update(&window, delta_time);

        engine.update_lighting_system(camera, window);
        window.clear_color({clear_col[0], clear_col[1], clear_col[2], clear_col[3]});

        window.draw(&skybox, &camera);

        skybox.attach_environment(*engine.default_program);
        skybox.attach_environment(*engine.default_circle_program);

        sphere.mesh->position.x = -14 + cos(timer) * 4;
        sphere.mesh->position.z = 0 + sin(timer) * 4;

        voxel_grid->update(&window, &camera);
        window.draw(voxel_grid, &camera);

        window.draw(&point_cloud_mesh, &camera);
        
        
        window.draw(&circle_cloud, &camera);
        window.draw(sphere.mesh, &camera);

        ui::begin_frame();
        ui::update_mouse_mode(&window);

        ImGui::Begin("Debug");

        glm::vec3 p = camera.position; // or any vec3

        ImGui::TextColored(ImVec4(1,0.5,0.5,1), "x: %.3f", p.x);
        ImGui::TextColored(ImVec4(0.5,1,0.5,1), "y: %.3f", p.y);
        ImGui::TextColored(ImVec4(0.5,0.5,1,1), "z: %.3f", p.z);
        ImGui::TextColored(ImVec4(0.5,0.5,1,1), "fps: %.3f", fps);

        ImGui::SliderFloat("Relative threshold", &rel_thresh, 0.0f, 10.0f);

        ImGui::End();

        ui::end_frame();
        

        window.swap_buffers();
        engine.poll_events();
    }
    
    ui::shutdown();
}
