#pragma once
#define NOMINMAX
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <set>
#include <filesystem>
#include <array>

#include "buffer_object.h"
#include "voxel_grid_gpu.h"
#include "imgui_layer.h"

class VoxelGridGPUDebugger {
public:
    std::shared_ptr<VoxelGridGPU> voxel_grid;
    std::shared_ptr<Window> window;

    bool voxel_grid_draw_streaming[3] = {false};
    std::array<std::function<void()>, 3> voxel_grid_draw_steps;
    std::string voxel_grid_draw_steps_names[3] = {"build_mesh_from_dirty()", "build_indirect_draw_commands_frustum_fn()", "draw_indirect()"};

    bool voxel_grid_generation_streaming[3] = {false};
    std::array<std::function<void()>, 3> voxel_grid_generation_steps;
    std::string voxel_grid_generation_steps_names[3] = {"mark_chunk_to_generate()", "generate_terrain()", "reset_load_list_counter()"};

    

    VoxelGridGPUDebugger(std::shared_ptr<VoxelGridGPU> voxel_grid, std::shared_ptr<Window> window);

    void print_counters();
    void print_count_free_mesh_alloc();
    void print_chunks_hash_table_log(); 
    void print_eviction_log(const glm::vec3& camera_pos); 

    void print_dirty_list();
    void print_dirty_list_emit_counters();
    void print_dirty_list_quad_count();
    void print_mesh_alloc_by_dirty_list(const std::string& prefix, uint32_t mesh_alloc_start_page_offset_bytes, uint32_t mesh_alloc_order_offset_bytes);

    void print_free_lists(
        const BufferObject& heads_buffer,
        const BufferObject& nodes_buffer,
        const BufferObject& states_buffer,
        uint32_t count_nodes,
        uint32_t count_pages,
        uint32_t max_order
    );

    void dispay_debug_window();
    void display_build_from_dirty_window();
    void display_build_cmd_window();
    void display_draw_pipline_window();
    void display_chunk_eviction_window();
    void display_stream_chunks_pipeline();
};