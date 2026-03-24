#define ALLOW_LOCK_BufferObject

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

#include "engine3d.h"
#include "imgui_layer.h"
#include "fps_camera_controller.h"
#include "voxel_grid_gpu.h"
#include "voxel_grid_gpu_debugger.h"
#include "torus.h"

#include "voxel_rasterizator_gpu.h"

float clear_col[4] = {0.776470588f, 0.988235294f, 1.0f, 1.0f};

BufferObject create_voxel_write_rect(glm::ivec3 origin, glm::uvec3 rect_dim, glm::vec3 color) {
    uint32_t count_voxels = rect_dim.x * rect_dim.y * rect_dim.z;
    std::vector<VoxelWriteGPU> voxels(count_voxels);

    uint32_t id = 0;
    for (uint32_t x = 0u; x < rect_dim.x; x++)
        for (uint32_t y = 0u; y < rect_dim.y; y++)
            for (uint32_t z = 0u; z < rect_dim.z; z++) {
                double ton = 0.5 + ((double)(rand() % 100000) / 100000) * 0.5;
                voxels[id].world_voxel = glm::ivec4(origin + glm::ivec3(x, y, z), 0);
                voxels[id].voxel_data = std::move(VoxelDataGPU(1, 1, 0, glm::ivec3(color * glm::vec3(ton))));
                voxels[id].set_flags = OVERWRITE_BIT;
                id++;
            }
    
    BufferObject voxel_writes(sizeof(uint32_t) * 4 + sizeof(VoxelWriteGPU) * count_voxels, GL_STATIC_COPY);
    voxel_writes.update_subdata(0, sizeof(uint32_t), &count_voxels);
    voxel_writes.update_subdata(sizeof(uint32_t) * 4, sizeof(VoxelWriteGPU) * count_voxels, voxels.data());

    return voxel_writes;
}

int main() {
    Engine3D engine = Engine3D();
    std::shared_ptr<Window> window = std::make_shared<Window>(&engine, 1280, 720, "3D visualization");
    engine.set_window(window.get());
    ui::init(window->window);
    
    window->disable_cursor();

    std::vector<std::filesystem::path> shaders_inlude_directories = {}; // Искать относительно executable_dir()
    ShaderManager shader_manager = ShaderManager(executable_dir(), shaders_inlude_directories);

    Camera camera;
    window->set_camera(&camera);

    FPSCameraController camera_controller = FPSCameraController(&camera);
    camera_controller.speed = 100;

    Mesh torus = Torus::create_torus_mesh();
    glm::vec3 torus_origin_pos = glm::vec3(0, 70, 0);
    torus.position = torus_origin_pos;
    torus.scale = glm::vec3(20);
    torus.rotation = glm::vec3(0, 0, 0);

    glm::vec3 voxel_size(1.0f);
    glm::ivec3 chunk_size(16);
    std::shared_ptr<VoxelGridGPU> voxel_grid_gpu = std::make_shared<VoxelGridGPU>(
        chunk_size, // chunk_size
        voxel_size, // voxel_size
        30'000, // count_active_chunks
        3'000'000, // max_quads
        4, // chunk_hash_table_size_factor
        32, // count_evict_buckets
        14'500, // min_free_chunks
        0.2f, // tomb_fraction_to_rebuild
        chunk_size.x * voxel_size.x * 1, // eviction_bucket_shell_thickness
        10, // vb_page_size_order_of_two
        10, // ib_page_size_order_of_two
        1.0, // buddy_allocator_nodes_factor
        chunk_size.x * voxel_size.x * 30, // render_distance
        15, // generation_distance
        chunk_size.x * chunk_size.x * 2'000, // max_write_count
        shader_manager
    );


    VoxelGridGPUDebugger voxel_grid_debugger(voxel_grid_gpu, window);
    VoxelRasterizatorGPU voxel_rasterizator(voxel_grid_gpu.get(), chunk_size, voxel_size, shader_manager);

    float rotation_speed = glm::pi<float>() / 2.0f;

    int y_offset = 0;
    float timer = 0;
    float lastFrame = 0;
    float wait_timer = 0;

    glm::vec3 torus_offset = glm::vec3(0);
    glm::ivec3 chunk_pos(0);
    while(window->is_open()) {
        float currentFrame = (float)glfwGetTime();
        float delta_time = currentFrame - lastFrame;
        timer += delta_time;
        wait_timer += delta_time;
        lastFrame = currentFrame;   

        ui::begin_frame();
        ui::update_mouse_mode(window.get());

        camera_controller.update(window.get(), delta_time);

        window->clear_color({clear_col[0], clear_col[1], clear_col[2], clear_col[3]});

        ///////////////////////////////////////////////////////////////////////////////////////////////////
        voxel_rasterizator.rasterize(torus, 1, 1, glm::ivec3(66, 135, 245), 0);
            
        voxel_grid_gpu->stream_chunks_sphere(camera_controller.camera->position, -1, 45345345);
        window->draw(voxel_grid_gpu.get(), &camera);
        // window->draw(&torus, &camera);

        voxel_rasterizator.rasterize(torus, 0, 0, glm::ivec3(66, 135, 245), 1);
        ///////////////////////////////////////////////////////////////////////////////////////////////////

        torus.rotation.x += rotation_speed * delta_time;
        torus.rotation.y += rotation_speed * delta_time;
        torus.position = torus_origin_pos + torus_offset;

        ImGui::Begin("Torus");
        ImGui::SliderFloat("Position X", &torus_offset.x, -1000, 1000);
        ImGui::SliderFloat("Position Y", &torus_offset.y, -1000, 1000);
        ImGui::SliderFloat("Position Z", &torus_offset.z, -1000, 1000);
        ImGui::End();

        voxel_grid_debugger.dispay_debug_window();
        voxel_grid_debugger.display_build_cmd_window();
        voxel_grid_debugger.display_build_from_dirty_window();
        voxel_grid_debugger.display_chunk_eviction_window();
        voxel_grid_debugger.display_draw_pipline_window();
        voxel_grid_debugger.display_stream_chunks_pipeline_window();
        voxel_grid_debugger.display_hash_table_window();

        ui::end_frame();

        window->swap_buffers();
        engine.poll_events();
    }
    
    ui::shutdown();
}
