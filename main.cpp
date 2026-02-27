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

float clear_col[4] = {0.15f, 0.15f, 0.18f, 1.0f};

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
    // Mesh point_cloud_mesh = point_cloud_frame.point_cloud.generate_mesh(1.5);
    
    Mesh point_cloud_mesh = point_cloud_frame.point_cloud.generate_mesh_gpu(rings_count, ring_size, 1.5);
    point_cloud_mesh.position = glm::vec3(30, 5, 10);
    point_cloud_mesh.scale = glm::vec3(5, 5, 5);

    int size = 50;
    for (int x = 0; x < size; x++) {
        for (int z = 0; z < size; z++) {
            int idx = z + x * size;
            float r = x / (float)size;
            float g = z / (float)size;
            glm::vec3 pos = glm::vec3(-x * 15, 10, -z * 15);
            voxel_grid->adjust_to_ground(pos);
            // engine.set_light_source(idx, {glm::vec4(pos.x, pos.y, pos.z, 10.0f), glm::vec4(r, g, 1.0f, 1.0f)});
            engine.lighting_system.set_light_source(idx, {glm::vec4(pos.x, pos.y + 5, pos.z, 30.0f), glm::vec4(0.9f, 0.9f, 1.0f, 1.0f)});    
        }
    }

    unsigned int num_clusters = engine.lighting_system.num_clusters.x * engine.lighting_system.num_clusters.y * engine.lighting_system.num_clusters.z;
    
    float rel_thresh = 1.5f;
    float last_frame = 0.0f;
    float timer = 0.0f;
    unsigned int counts[num_clusters];
    while(window.is_open()) {
        float currentFrame = (float)glfwGetTime();
        float delta_time = currentFrame - last_frame;
        timer += delta_time;
        last_frame = currentFrame;   
        float fps = 1.0f / delta_time;
        
        camera_controller.update(&window, delta_time);

        engine.update_lighting_system(camera, window);
        window.clear_color({clear_col[0], clear_col[1], clear_col[2], clear_col[3]});

        voxel_grid->update(&window, &camera);
        window.draw(voxel_grid, &camera);

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
