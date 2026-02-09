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

void create_voxels_box(
    glm::ivec3 box_origin, 
    glm::ivec3 box_dim, 
    glm::vec3 color_a, 
    glm::vec3 color_b, 
    double p_vis, 
    std::vector<glm::ivec3>& voxel_positions_out,
    std::vector<VoxelGridGPU::VoxelDataGPU> voxels_out
) {
    glm::vec3 box_center = glm::vec3(box_origin) + glm::vec3(box_dim) * glm::vec3(0.5);
    double max_lenght = glm::length(glm::vec3(box_dim)) / 2.0;
    
    voxel_positions_out.resize(box_dim.x * box_dim.y * box_dim.z);
    voxels_out.resize(box_dim.x * box_dim.y * box_dim.z);

    int id = 0;
    for (int x = 0; x < box_dim.x; x++)
        for (int y = 0; y < box_dim.y; y++)
            for (int z = 0; z < box_dim.z; z++) {
                uint32_t vis = 0;
                if (p_vis < (rand() % 10000) / 10000.0) {
                    vis = 1;
                }
                
                glm::vec3 current_pos = glm::vec3(x, y, z);
                double dis = glm::length(box_center - current_pos);
                double t = dis / max_lenght;
                glm::vec3 color = color_b + (color_a - color_b) * glm::vec3(t);
                glm::ivec3 icolor = color * glm::vec3(255);

                voxel_positions_out[id] = box_origin + glm::ivec3(x, y, z);
                // voxels[id].color = (icolor.x << 24) | (icolor.y << 16) | (icolor.z << 8) | 0xffu;
                voxels_out[id].color = (255 << 24) | (icolor.z << 16) | (icolor.y << 8) | icolor.x;
                voxels_out[id].type_vis_flags |= (vis & 0xFFu) << 16; // тип 1
                voxels_out[id].type_vis_flags |= (vis & 0xFFu) << 8; // не прозрачный
                id++;
            }
}

void create_occlusion_test(
    glm::ivec3 origin, 
    glm::ivec3 right_basis, 
    glm::ivec3 up_basis, 
    glm::ivec3 front_basis, 
    std::vector<glm::ivec3>& positions_out, 
    std::vector<VoxelGridGPU::VoxelDataGPU>& voxels_out
) {
    glm::imat3x3 transform = {right_basis, up_basis, front_basis};

    glm::ivec3 color = glm::ivec3(255, 255, 255);
    uint32_t count_changing_voxels = 8;
    uint32_t count_samples = 1 << count_changing_voxels;
    uint32_t side_size = count_samples >> (count_changing_voxels / 2);
    glm::ivec3 local_offsets[8] = {
        glm::ivec3(0, 0, 0),
        glm::ivec3(1, 0, 0),
        glm::ivec3(2, 0, 0),
        glm::ivec3(2, 1, 0),
        glm::ivec3(2, 2, 0),
        glm::ivec3(1, 2, 0),
        glm::ivec3(0, 2, 0),
        glm::ivec3(0, 1, 0)
    };

    voxels_out.reserve(count_samples * count_changing_voxels + count_samples);
    positions_out.reserve(count_samples * count_changing_voxels + count_samples);
    
    uint32_t id = 0;
    for (uint32_t x = 0; x < side_size; x++)
        for (uint32_t y = 0; y < side_size; y++) {
            for (uint32_t i = 0; i < count_changing_voxels; i++) {
                uint32_t vis = (id >> i) & 1u; // узнаём бит

                VoxelGridGPU::VoxelDataGPU side_voxel = {0};
                side_voxel.color = (255 << 24) | (color.z << 16) | (color.y << 8) | color.x;
                side_voxel.type_vis_flags |= (vis & 0xFFu) << 16; // тип 1
                side_voxel.type_vis_flags |= (vis & 0xFFu) << 8; // не прозрачный
                
                glm::ivec3 local_offset = local_offsets[i];
                glm::ivec3 side_voxel_position = glm::ivec3(x * 4, y * 4, 0) + local_offset;

                glm::ivec3 side_voxel_position_in_new_basis = origin + transform * side_voxel_position;

                voxels_out.push_back(side_voxel);
                positions_out.push_back(side_voxel_position_in_new_basis);
            }
            
            VoxelGridGPU::VoxelDataGPU center_voxel = {0};
            center_voxel.color = (255 << 24) | (color.z << 16) | (color.y << 8) | color.x;
            center_voxel.type_vis_flags |= (1u & 0xFFu) << 16; // тип 1
            center_voxel.type_vis_flags |= (1u & 0xFFu) << 8; // не прозрачный

            glm::ivec3 center_voxel_position = glm::ivec3(x * 4 + 1, y * 4 + 1, 1);
            glm::ivec3 center_voxel_position_in_new_basis = origin + transform * center_voxel_position;

            voxels_out.push_back(center_voxel);
            positions_out.push_back(center_voxel_position_in_new_basis);
            id++;
        }
}

void create_occlusion_test_box(glm::ivec3 origin, std::vector<glm::ivec3>& positions_out, std::vector<VoxelGridGPU::VoxelDataGPU>& voxels_out) {
    std::vector<glm::ivec3> positions_PZ;
    std::vector<glm::ivec3> positions_Z;
    std::vector<glm::ivec3> positions_PX;
    std::vector<glm::ivec3> positions_X;
    std::vector<glm::ivec3> positions_PY;
    std::vector<glm::ivec3> positions_Y;

    std::vector<VoxelGridGPU::VoxelDataGPU> voxels_PZ;
    std::vector<VoxelGridGPU::VoxelDataGPU> voxels_Z;
    std::vector<VoxelGridGPU::VoxelDataGPU> voxels_PX;
    std::vector<VoxelGridGPU::VoxelDataGPU> voxels_X;
    std::vector<VoxelGridGPU::VoxelDataGPU> voxels_PY;
    std::vector<VoxelGridGPU::VoxelDataGPU> voxels_Y;

    uint32_t count_changing_voxels = 8;
    uint32_t count_samples = 1 << count_changing_voxels;
    uint32_t side_count = count_samples >> (count_changing_voxels / 2);
    uint32_t side_size = side_count * 4;
    glm::ivec3 rt_origin = {side_size, side_size, side_size};

    create_occlusion_test({0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, positions_Z, voxels_Z);
    create_occlusion_test({0, 0, 0}, {0, 0, 1}, {0, 1, 0}, {1, 0, 0}, positions_PX, voxels_PX);
    create_occlusion_test({0, 0, 0}, {1, 0, 0}, {0, 0, 1}, {0, 1, 0}, positions_Y, voxels_Y);
    create_occlusion_test(rt_origin, {-1, 0, 0}, {0, -1, 0}, {0, 0, -1}, positions_PZ, voxels_PZ);
    create_occlusion_test(rt_origin, {0, 0, -1}, {0, -1, 0}, {-1, 0, 0}, positions_X, voxels_X);
    create_occlusion_test(rt_origin, {-1, 0, 0}, {0, 0, -1}, {0, -1, 0}, positions_PY, voxels_PY);

    std::vector<glm::ivec3> positions;
    positions.reserve((count_samples * count_changing_voxels + count_changing_voxels) * 6);

    std::vector<VoxelGridGPU::VoxelDataGPU> voxels;
    voxels.reserve((count_samples * count_changing_voxels + count_changing_voxels) * 6);

    positions.insert(positions.end(), positions_PX.begin(), positions_PX.end());
    positions.insert(positions.end(), positions_X.begin(), positions_X.end());
    positions.insert(positions.end(), positions_PY.begin(), positions_PY.end());
    positions.insert(positions.end(), positions_Y.begin(), positions_Y.end());
    positions.insert(positions.end(), positions_PZ.begin(), positions_PZ.end());
    positions.insert(positions.end(), positions_Z.begin(), positions_Z.end());

    voxels.insert(voxels.end(), voxels_PX.begin(), voxels_PX.end());
    voxels.insert(voxels.end(), voxels_X.begin(), voxels_X.end());
    voxels.insert(voxels.end(), voxels_PY.begin(), voxels_PY.end());
    voxels.insert(voxels.end(), voxels_Y.begin(), voxels_Y.end());
    voxels.insert(voxels.end(), voxels_PZ.begin(), voxels_PZ.end());
    voxels.insert(voxels.end(), voxels_Z.begin(), voxels_Z.end());

    positions_out = std::move(positions);
    voxels_out = std::move(voxels);
}

float clear_col[4] = {0.776470588f, 0.988235294f, 1.0f, 1.0f};
// float clear_col[4] = {0.0f, 0.0f, 0.0f, 1.0f};

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

    std::vector<glm::ivec3> positions;
    std::vector<VoxelGridGPU::VoxelDataGPU> voxels;
    create_occlusion_test_box({0, 0, 0}, positions, voxels);
    voxel_grid_gpu.apply_writes_to_world_from_cpu(positions, voxels);

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
