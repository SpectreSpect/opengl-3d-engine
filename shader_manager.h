#pragma once
#include <iostream>
#include <string>
#include <filesystem>

#include "compute_shader.h"
#include "vertex_shader.h"
#include "fragment_shader.h"
#include <vector>

namespace fs = std::filesystem;

class ShaderManager {
public:
    // General
    ComputeShader dispatch_adapter_cs;

    //voxelizaton
    ComputeShader count_cs;
    ComputeShader scan_blocks_cs;
    ComputeShader add_block_offsets_cs;
    ComputeShader fix_last_cs;
    ComputeShader copy_offsets_to_cursor_cs;
    ComputeShader fill_cs;
    ComputeShader voxelize_cs;
    ComputeShader clear_cs;
    ComputeShader roi_reduce_indices_cs;
    ComputeShader roi_reduce_pairs_cs;
    ComputeShader build_active_chunks_cs;
    ComputeShader roi_finalize_cs;

    //voxel_grid
    ComputeShader clear_chunks_cs;
    ComputeShader set_chunks_cs;
    ComputeShader world_init_cs;
    ComputeShader apply_writes_to_world_cs;
    ComputeShader mesh_counters_reset_cs;
    ComputeShader mesh_reset_cs;
    ComputeShader mesh_count_cs;
    ComputeShader mesh_alloc_cs;
    ComputeShader mesh_emit_cs;
    ComputeShader mesh_finalize_cs;
    ComputeShader cmdcount_reset_cs;
    ComputeShader build_indirect_cmds_cs;
    ComputeShader reset_dirty_count_cs;
    ComputeShader bucket_reset_cs;
    ComputeShader bucket_build_cs;
    ComputeShader evict_lowprio_cs;
    ComputeShader stream_select_chunks_cs;
    ComputeShader stream_generate_terrain_cs;
    ComputeShader mark_all_user_chunks_as_dirty_cs;
    ComputeShader mesh_pool_clear_cs;
    ComputeShader mesh_pool_seed_cs;
    ComputeShader reset_load_list_counter_cs;
    ComputeShader verify_mesh_allocation_cs;
    ComputeShader return_free_alloc_nodes_cs;
    ComputeShader return_free_alloc_nodes_dispatch_adapter_cs;
    ComputeShader free_evicted_chunks_mesh_cs;
    ComputeShader fill_chunk_hash_table_cs;
    ComputeShader clear_chunk_hash_table_cs;
    VertexShader voxel_mesh_vs;
    FragmentShader voxel_mesh_fs;

    ShaderManager(const std::string& root_path);
    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;
    ShaderManager(ShaderManager&&) noexcept = default;
    ShaderManager& operator=(ShaderManager&&) noexcept = default;

    void init_shaders(const std::string& root_path);
};