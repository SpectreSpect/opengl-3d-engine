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
#include "cube.h"

float clear_col[4] = {0.776470588f, 0.988235294f, 1.0f, 1.0f};

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

    Torus torus(glm::vec3(0), glm::vec3(10), glm::vec3(0));

    float voxel_size = 1.0f;
    uint32_t chunk_size = 16u;
    std::shared_ptr<VoxelGridGPU> voxel_grid_gpu = std::make_shared<VoxelGridGPU>(
        glm::ivec3(chunk_size), // chunk_size
        glm::vec3(voxel_size), // voxel_size
        30'000, // count_active_chunks
        3'000'000, // max_quads
        4, // chunk_hash_table_size_factor
        4096, // count_evict_buckets
        10'000, // min_free_chunks
        0.2f, // tomb_fraction_to_rebuild
        chunk_size * voxel_size * 1, // eviction_bucket_shell_thickness
        10, // vb_page_size_order_of_two
        10, // ib_page_size_order_of_two
        1.0, // buddy_allocator_nodes_factor
        chunk_size * voxel_size * 30,
        shader_manager
    );

    VoxelGridGPUDebugger voxel_grid_debugger(voxel_grid_gpu, window);

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

    float timer = 0;
    float lastFrame = 0;
    while(window->is_open()) {
        float currentFrame = (float)glfwGetTime();
        float delta_time = currentFrame - lastFrame;
        timer += delta_time;
        lastFrame = currentFrame;   

        ui::begin_frame();
        ui::update_mouse_mode(window.get());

        camera_controller.update(window.get(), delta_time);

        window->clear_color({clear_col[0], clear_col[1], clear_col[2], clear_col[3]});

        voxel_grid_gpu->stream_chunks_sphere(camera_controller.camera->position, 10, 45345345);
        window->draw(voxel_grid_gpu.get(), &camera);
        window->draw(&torus, &camera);

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
