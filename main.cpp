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

float clear_col[4] = {0.776470588f, 0.988235294f, 1.0f, 1.0f};

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
    camera_controller.speed = 20;

    VoxelGrid voxel_grid = VoxelGrid({16, 16, 16}, 1.0f, {24, 6, 24});
    float chunk_render_size = voxel_grid.chunk_size.x * voxel_grid.voxel_size;

    VoxelRasterizatorGPU voxel_rastorizator(&voxel_grid, shader_manager);

    VertexLayout vertex_layout = VertexLayout::create_default_vertex_layout();

    VtkMeshLoader vtk_mesh_loader = VtkMeshLoader(vertex_layout);

    MeshData model_mesh_data = vtk_mesh_loader.load_mesh((executable_dir() / "models" / "test_mesh.vtk").string());
    Mesh model = Mesh(model_mesh_data.vertices, model_mesh_data.indices, &vertex_layout); 
    model.position = {chunk_render_size * 0, chunk_render_size * 5, chunk_render_size * 0};
    model.scale = glm::vec3(5.0f);

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

        voxel_grid.update(&window, &camera);
        window.draw(&voxel_grid, &camera);
        window.draw(&model, &camera);

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

            voxel_rastorizator.rasterize(model, voxel_grid.voxel_size, voxel_grid.chunk_size.x);
        }


        ImGui::End();

        ui::end_frame();

        window.swap_buffers();
        engine.poll_events();

        prev_cam_pos = camera_controller.camera->position;
    }
    
    ui::shutdown();
}
