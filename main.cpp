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
#include "vtk_mesh_loader.h"
#include "path_utils.h"
#include "voxel_rasterizator_gpu.h"
#include "shader_manager.h"
#include "voxel_grid_gpu.h"

// float clear_col[4] = {0.776470588f, 0.988235294f, 1.0f, 1.0f};
float clear_col[4] = {0.0f, 0.0f, 0.0f, 1.0f};

int main() {
    Engine3D engine = Engine3D();
    Window window = Window(&engine, 1280, 720, "3D visualization");
    engine.set_window(&window);
    ui::init(window.window);
    
    window.disable_cursor();

    ShaderManager shader_manager = ShaderManager(executable_dir_str());

    Camera camera;
    window.set_camera(&camera);

    FPSCameraController camera_controller = FPSCameraController(&camera);
    // camera_controller.speed = 20;
    camera_controller.speed = 100;

    // VoxelGrid voxel_grid = VoxelGrid({16, 16, 16}, 1.0f, {24, 6, 24});
    // float chunk_render_size = voxel_grid.chunk_size.x * voxel_grid.voxel_size;

    // VoxelRasterizatorGPU voxel_rastorizator(&voxel_grid, shader_manager);

    // VtkMeshLoader vtk_mesh_loader = VtkMeshLoader(vertex_layout);

    // MeshData model_mesh_data = vtk_mesh_loader.load_mesh((executable_dir() / "models" / "test_mesh.vtk").string());
    // Mesh model = Mesh(model_mesh_data.vertices, model_mesh_data.indices, &vertex_layout); 
    // model.position = {chunk_render_size * 0, chunk_render_size * 5, chunk_render_size * 0};
    // model.scale = glm::vec3(5.0f);

    VoxelGridGPU voxel_grid_gpu = VoxelGridGPU(
        {16, 16, 16}, // chunk_size
        {1.0f, 1.0f, 1.0f}, // voxel_size
        10'000, // count_active_chunks
        30'000'000, // max_quads
        4, //chunk_hash_table_size_factor
        512, //max_count_probing
        shader_manager
    );

    glm::ivec3 box_origin = {0, 0, 0};
    glm::ivec3 box_dim = {300, 300, 300};
    glm::vec3 box_center = glm::vec3(box_origin) + glm::vec3(box_dim) * glm::vec3(0.5);
    double max_lenght = glm::length(glm::vec3(box_dim)) / 2.0;
    glm::vec3 color_a = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 color_b = glm::vec3(1.0f, 0.0f, 0.0f);
    
    std::vector<glm::ivec3> voxel_positions;
    voxel_positions.resize(box_dim.x * box_dim.y * box_dim.z);

    std::vector<VoxelGridGPU::VoxelDataGPU> voxels;
    voxels.resize(box_dim.x * box_dim.y * box_dim.z);

    int id = 0;
    for (int x = 0; x < box_dim.x; x++)
        for (int y = 0; y < box_dim.y; y++)
            for (int z = 0; z < box_dim.z; z++) {
                uint32_t vis = 0;
                if ((rand() % 10000) / 10000.0 > 0.98) {
                    vis = 1;
                }
                
                glm::vec3 current_pos = glm::vec3(x, y, z);
                double dis = glm::length(box_center - current_pos);
                double t = dis / max_lenght;
                glm::vec3 color = color_b + (color_a - color_b) * glm::vec3(t);
                glm::ivec3 icolor = color * glm::vec3(255);

                voxel_positions[id] = box_origin + glm::ivec3(x, y, z);
                // voxels[id].color = (icolor.x << 24) | (icolor.y << 16) | (icolor.z << 8) | 0xffu;
                voxels[id].color = (255 << 24) | (icolor.z << 16) | (icolor.y << 8) | icolor.x;
                voxels[id].type_vis_flags |= (vis & 0xFFu) << 16; // тип 1
                voxels[id].type_vis_flags |= (vis & 0xFFu) << 8; // не прозрачный
                id++;
            }

    voxel_grid_gpu.apply_writes_to_world_from_cpu(voxel_positions, voxels);


    glm::vec3 prev_cam_pos = camera_controller.camera->position;

    float timer = 0;
    float lastFrame = 0;
    while(window.is_open()) {
        float currentFrame = (float)glfwGetTime();
        float delta_time = currentFrame - lastFrame;
        timer += delta_time;
        lastFrame = currentFrame;   

        ui::begin_frame();
        ui::update_mouse_mode(&window);

        camera_controller.update(&window, delta_time);

        window.clear_color({clear_col[0], clear_col[1], clear_col[2], clear_col[3]});

        // voxel_grid.update(&window, &camera);
        // window.draw(&voxel_grid, &camera);
        // window.draw(&model, &camera);
        
         window.draw(&voxel_grid_gpu, &camera);

        ImGui::Begin("Debug");

        ImGui::TextUnformatted("Camera position");

        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 220, 120, 255));
        ImGui::Text("x: %.3f", camera_controller.camera->position.x);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 255, 120, 255));
        ImGui::Text("y: %.3f", camera_controller.camera->position.y);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 180, 255, 255));
        ImGui::Text("z: %.3f", camera_controller.camera->position.z);
        ImGui::PopStyleColor();

        if (ImGui::Button("Rasterize the triangle")) {
            // MeshData mesh_data = cube->create_mesh_data(cube->get_color());

            // auto voxel_generator = [&](glm::vec3 point) -> Voxel {
            //     Voxel voxel;
            //     voxel.color = glm::vec3(1.0f, 0.0f, 0.0f);
            //     voxel.visible = rand() % 10000 > 5000;
            //     return voxel;
            // };

            // voxel_rastorizator.rasterize(model, voxel_grid.voxel_size, voxel_grid.chunk_size.x);

            // voxel_grid_gpu.print_counters(1, 1, 1, 1);
            // std::cout << std::endl;
            // window.draw(&voxel_grid_gpu, &camera);
            // voxel_grid_gpu.print_counters(1, 1, 1, 1);
            // std::cout << "-----------------------" << std::endl << std::endl;
        }


        ImGui::End();

        ui::end_frame();

        window.swap_buffers();
        engine.poll_events();

        prev_cam_pos = camera_controller.camera->position;
    }
    
    ui::shutdown();
}
