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

    float voxel_size = 1.0f;
    uint32_t chunk_size = 16u;
    std::shared_ptr<VoxelGridGPU> voxel_grid_gpu = std::make_shared<VoxelGridGPU>(
        glm::ivec3(chunk_size), // chunk_size
        glm::vec3(voxel_size), // voxel_size
        30'000, // count_active_chunks
        3'000'000, // max_quads
        4, // chunk_hash_table_size_factor
        4096, // count_evict_buckets
        15'000, // min_free_chunks
        0.2f, // tomb_fraction_to_rebuild
        chunk_size * voxel_size * 1, // eviction_bucket_shell_thickness
        10, // vb_page_size_order_of_two
        10, // ib_page_size_order_of_two
        1.0, // buddy_allocator_nodes_factor
        chunk_size * voxel_size * 30, // render_distance
        chunk_size * chunk_size * 2'000, // max_write_count
        shader_manager
    );


    VoxelGridGPUDebugger voxel_grid_debugger(voxel_grid_gpu, window);
    VoxelRasterizatorGPU voxel_rasterizator(voxel_grid_gpu.get(), shader_manager);

    // BufferObject write_voxels = create_voxel_write_rect(glm::ivec3(0, 47, 0), glm::ivec3(1, 1, 1), glm::ivec3(66, 135, 245));
    // voxel_grid_gpu->set_voxels(write_voxels);

    // ////////////////////////////////////////////////
    // // voxel_grid_gpu->ensure_free_chunks_gpu(window->camera->position, math_utils::BITS, math_utils::OFFSET);
    // // voxel_grid_gpu->reset_load_list_counter();

    // // voxel_grid_gpu->mark_chunk_to_generate(window->camera->position, 10);

    // // voxel_grid_gpu->merge_voxel_write_lists(voxel_grid_gpu->local_voxel_write_list_, voxel_grid_gpu->voxel_write_list_);
    // // voxel_grid_gpu->reset_voxel_write_list_counter(voxel_grid_gpu->local_voxel_write_list_);
    // // voxel_grid_gpu->prepare_dispatch_args(voxel_grid_gpu->dispatch_args, BufferDispatchArg(&voxel_grid_gpu->voxel_write_list_, 0));
    // // voxel_grid_gpu->mark_write_chunks_to_generate(voxel_grid_gpu->dispatch_args);


    // // voxel_grid_gpu->prepare_dispatch_args(voxel_grid_gpu->dispatch_args, ValueDispatchArg(voxel_grid_gpu->vox_per_chunk), BufferDispatchArg(&voxel_grid_gpu->load_list_, 0u));
    // // voxel_grid_gpu->generate_terrain(voxel_grid_gpu->dispatch_args, 45345345);

    // // voxel_grid_gpu->prepare_dispatch_args(voxel_grid_gpu->dispatch_args, BufferDispatchArg(&voxel_grid_gpu->voxel_write_list_, 0u));
    // // voxel_grid_gpu->write_voxels_to_grid();

    // // voxel_grid_gpu->reset_voxel_write_list_counter(voxel_grid_gpu->voxel_write_list_);
    // ////////////////////////////////////////////////

    // voxel_grid_gpu->stream_chunks_sphere(camera_controller.camera->position, 10, 45345345);
    // voxel_grid_gpu->build_mesh_from_dirty(math_utils::BITS, math_utils::OFFSET);



    // write_voxels = create_voxel_write_rect(glm::ivec3(0, 48, 0), glm::ivec3(1, 1, 1), glm::ivec3(66, 135, 245));
    // voxel_grid_gpu->set_voxels(write_voxels);

    glm::vec3 prev_cam_pos = camera_controller.camera->position;

    std::filesystem::path state_dumps_dir = executable_dir() / "voxel_grid" / "state_dumps";
    std::filesystem::path dumps_dir = executable_dir() / "voxel_grid" / "dumps";
    std::filesystem::path base_dump_path = dumps_dir / "base_dump";
    std::filesystem::path dump_animations_path = dumps_dir / "dump_animations";
    std::filesystem::path frames_path_to_load = dump_animations_path / "to_load";

    int selected_frame_id = 0;
    int offset_verify_stack = 0;
    int count_elements_verify_stack = -1;
    int dirty_count_to_set = 0;
    bool use_verify_stack = false;

    float rotation_speed = glm::pi<float>() / 2.0f;

    int y_offset = 0;
    float timer = 0;
    float lastFrame = 0;
    float rast_timer = 0;
    float wait_timer = 0;

    glm::vec3 torus_offset = glm::vec3(0); 

    float rasterise_time = 0.0;
    while(window->is_open()) {
        float currentFrame = (float)glfwGetTime();
        float delta_time = currentFrame - lastFrame;
        timer += delta_time;
        rast_timer += delta_time;
        wait_timer += delta_time;
        lastFrame = currentFrame;   

        ui::begin_frame();
        ui::update_mouse_mode(window.get());

        camera_controller.update(window.get(), delta_time);

        window->clear_color({clear_col[0], clear_col[1], clear_col[2], clear_col[3]});

        // if (rast_timer >= rasterise_time)
            
        
        // VoxelGridGPU::debug = true;
        voxel_grid_gpu->stream_chunks_sphere(camera_controller.camera->position, 10, 45345345);
        window->draw(voxel_grid_gpu.get(), &camera);
        // break;

        // window->draw(&torus, &camera);

        ImGui::Begin("Torus");
        ImGui::SliderFloat("Position X", &torus_offset.x, -1000, 1000);
        ImGui::SliderFloat("Position Y", &torus_offset.y, -1000, 1000);
        ImGui::SliderFloat("Position Z", &torus_offset.z, -1000, 1000);
        ImGui::End();

        if (rast_timer >= rasterise_time)
            voxel_rasterizator.rasterize(torus, voxel_size, chunk_size, 0, 0, glm::ivec3(66, 135, 245), 1);

        if (rast_timer >= rasterise_time) {
            torus.rotation.x += rotation_speed * rast_timer;
            torus.rotation.y += rotation_speed * rast_timer;
            torus.position = torus_origin_pos + torus_offset;

            rast_timer = 0;

            voxel_rasterizator.rasterize(torus, voxel_size, chunk_size, 1, 1, glm::ivec3(66, 135, 245), 0);
        }

        // if (wait_timer > 5.0f) {
        //     torus.rotation.x = ((rand() % 10000) / 10000.0) * glm::two_pi<double>();
        //     torus.rotation.y = ((rand() % 10000) / 10000.0) * glm::two_pi<double>();
        //     wait_timer = 0.0f;
        // }
        


        voxel_grid_debugger.dispay_debug_window();
        voxel_grid_debugger.display_build_cmd_window();
        voxel_grid_debugger.display_build_from_dirty_window();
        voxel_grid_debugger.display_chunk_eviction_window();
        voxel_grid_debugger.display_draw_pipline_window();
        voxel_grid_debugger.display_stream_chunks_pipeline();

        ui::end_frame();

        window->swap_buffers();
        engine.poll_events();

        prev_cam_pos = camera_controller.camera->position;
    }
    
    ui::shutdown();
}
