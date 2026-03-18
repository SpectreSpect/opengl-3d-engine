#pragma once
#include <iostream>
#include <string>
#include <filesystem>

#include "compute_shader.h"
#include "vertex_shader.h"
#include "fragment_shader.h"
#include "compute_program.h"
#include <vector>

namespace fs = std::filesystem;

class ShaderManager {
public:
    std::vector<std::filesystem::path> include_directories;

    // General
    ComputeShader dispatch_adapter_cs;
    ComputeShader clear_buffer_cs;
    ComputeProgram clear_buffer_prog;

    //voxelizaton
    ComputeShader count_triangles_in_chunks_cs;
    ComputeShader scan_blocks_cs;
    ComputeShader add_block_offsets_cs;
    ComputeShader fix_last_cs;
    ComputeShader copy_offsets_to_cursor_cs;
    ComputeShader fill_triangle_indices_cs;
    ComputeShader voxelize_cs;
    ComputeShader roi_reduce_indices_cs;
    ComputeShader roi_reduce_pairs_cs;
    ComputeShader build_active_chunks_cs;
    ComputeShader roi_finalize_cs;

    //voxel_grid
    ComputeShader clear_chunks_cs;
    ComputeShader world_init_cs;
    ComputeShader apply_writes_to_world_cs;
    ComputeShader mesh_counters_reset_cs;
    ComputeShader mesh_reset_cs;
    ComputeShader mesh_count_cs;
    ComputeShader mesh_alloc_cs;
    ComputeShader mesh_emit_cs;
    ComputeShader mesh_finalize_cs;
    ComputeShader build_indirect_cmds_cs;
    ComputeShader reset_dirty_count_cs;
    ComputeShader evict_buckets_build_cs;
    ComputeShader evict_low_priority_cs;
    ComputeShader evict_low_priority_dispatch_adapter_cs;
    ComputeShader stream_select_chunks_cs;
    ComputeShader stream_generate_terrain_cs;
    ComputeShader mark_all_user_chunks_as_dirty_cs;
    ComputeShader mesh_pool_clear_cs;
    ComputeShader mesh_pool_seed_cs;
    ComputeShader verify_mesh_allocation_cs;
    ComputeShader return_free_alloc_nodes_cs;
    ComputeShader return_free_alloc_nodes_dispatch_adapter_cs;
    ComputeShader free_evicted_chunks_mesh_cs;
    ComputeShader fill_chunk_hash_table_cs;
    ComputeShader clear_chunk_hash_table_cs;
    ComputeShader reset_evicted_list_and_buckets_cs;
    ComputeShader hash_table_conditional_dispatch_adapter_cs;
    ComputeShader set_voxels_cs;
    VertexShader voxel_mesh_vs;
    FragmentShader voxel_mesh_fs;

    ShaderManager(const std::filesystem::path& root_path, std::vector<std::filesystem::path> include_directories = std::vector<std::filesystem::path>());
    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;
    ShaderManager(ShaderManager&&) noexcept = default;
    ShaderManager& operator=(ShaderManager&&) noexcept = default;

    void add_include_directory(std::filesystem::path directory);

    void init_shaders(const std::filesystem::path& root_path);
};