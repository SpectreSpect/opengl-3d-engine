#include "shader_manager.h"

ShaderManager::ShaderManager(const std::string& root_path) {
    init_shaders(root_path);
}

void ShaderManager::init_shaders(const std::string& root_path) {
    fs::path p(root_path);
    dispatch_adapter_cs = ComputeShader((p / "shaders" / "dispatch_adapter.glsl").string());

    count_cs = ComputeShader((p / "shaders" / "voxel_rasterization" / "count_cs.glsl").string()); 
    scan_blocks_cs = ComputeShader((p / "shaders" / "voxel_rasterization" / "scan_blocks_cs.glsl").string());
    add_block_offsets_cs = ComputeShader((p / "shaders" / "voxel_rasterization" / "add_block_offsets_cs.glsl").string());
    fix_last_cs = ComputeShader((p / "shaders" / "voxel_rasterization" / "fix_last_cs.glsl").string());
    copy_offsets_to_cursor_cs = ComputeShader((p / "shaders" / "voxel_rasterization" / "copy_offsets_to_cursor_cs.glsl").string());
    fill_cs = ComputeShader((p / "shaders" / "voxel_rasterization" / "fill_cs.glsl").string());
    voxelize_cs = ComputeShader((p / "shaders" / "voxel_rasterization" / "voxelize_cs.glsl").string());
    clear_cs = ComputeShader((p / "shaders" / "voxel_rasterization" / "clear_cs.glsl").string());
    roi_reduce_indices_cs = ComputeShader((p / "shaders" / "voxel_rasterization" / "roi_reduce_indices_cs.glsl").string());
    roi_reduce_pairs_cs = ComputeShader((p / "shaders" / "voxel_rasterization" / "roi_reduce_pairs_cs.glsl").string());
    build_active_chunks_cs = ComputeShader((p / "shaders" / "voxel_rasterization" / "build_active_chunks_cs.glsl").string());
    roi_finalize_cs = ComputeShader((p / "shaders" / "voxel_rasterization" / "roi_finalize_cs.glsl").string());
    clear_chunks_cs = ComputeShader((p / "shaders" / "voxel_grid" / "clear_chunks.glsl").string());
    set_chunks_cs = ComputeShader((p / "shaders" / "voxel_grid" / "set_chunks.glsl").string());
    world_init_cs = ComputeShader((p / "shaders" / "voxel_grid" / "world_init.glsl").string());
    apply_writes_to_world_cs = ComputeShader((p / "shaders" / "voxel_grid" / "apply_writes_to_world.glsl").string());
    mesh_counters_reset_cs = ComputeShader((p / "shaders" / "voxel_grid" / "mesh_counters_reset.glsl").string());
    mesh_reset_cs = ComputeShader((p / "shaders" / "voxel_grid" / "mesh_reset.glsl").string());
    mesh_count_cs = ComputeShader((p / "shaders" / "voxel_grid" / "mesh_count.glsl").string());
    mesh_alloc_cs = ComputeShader((p / "shaders" / "voxel_grid" / "mesh_alloc.glsl").string());
    mesh_emit_cs = ComputeShader((p / "shaders" / "voxel_grid" / "mesh_emit.glsl").string());
    mesh_finalize_cs = ComputeShader((p / "shaders" / "voxel_grid" / "mesh_finalize.glsl").string());
    cmdcount_reset_cs = ComputeShader((p / "shaders" / "voxel_grid" / "cmdcount_reset.glsl").string());
    build_indirect_cmds_cs = ComputeShader((p / "shaders" / "voxel_grid" / "build_indirect_cmds.glsl").string());
    reset_dirty_count_cs = ComputeShader((p / "shaders" / "voxel_grid" / "reset_dirty_count.glsl").string());
    bucket_reset_cs = ComputeShader((p / "shaders" / "voxel_grid" / "evict_buckets_reset.glsl").string());
    bucket_build_cs = ComputeShader((p / "shaders" / "voxel_grid" / "evict_buckets_build.glsl").string());
    evict_lowprio_cs = ComputeShader((p / "shaders" / "voxel_grid" / "evict_low_priority.glsl").string());
    evict_low_priority_dispatch_adapter_cs = ComputeShader((p / "shaders" / "voxel_grid" / "evict_low_priority_dispatch_adapter.glsl").string());
    stream_select_chunks_cs = ComputeShader((p / "shaders" / "voxel_grid" / "stream_select_chunks.glsl").string());
    stream_generate_terrain_cs = ComputeShader((p / "shaders" / "voxel_grid" / "stream_generate_terrain.glsl").string());
    mark_all_user_chunks_as_dirty_cs = ComputeShader((p / "shaders" / "voxel_grid" / "mark_all_user_chunks_as_dirty.glsl").string());
    mesh_pool_clear_cs = ComputeShader((p / "shaders" / "voxel_grid" / "mesh_pool_clear.glsl").string());
    mesh_pool_seed_cs = ComputeShader((p / "shaders" / "voxel_grid" / "mesh_pool_seed.glsl").string());
    reset_load_list_counter_cs = ComputeShader((p / "shaders" / "voxel_grid" / "reset_load_list_counter.glsl").string());
    verify_mesh_allocation_cs = ComputeShader((p / "shaders" / "voxel_grid" / "verify_mesh_allocation.glsl").string());
    return_free_alloc_nodes_cs = ComputeShader((p / "shaders" / "voxel_grid" / "return_free_alloc_nodes.glsl").string());
    return_free_alloc_nodes_dispatch_adapter_cs = ComputeShader((p / "shaders" / "voxel_grid" / "return_free_alloc_nodes_dispatch_adapter.glsl").string());
    free_evicted_chunks_mesh_cs = ComputeShader((p / "shaders" / "voxel_grid" / "free_evicted_chunks_mesh.glsl").string());
    fill_chunk_hash_table_cs = ComputeShader((p / "shaders" / "voxel_grid" / "fill_chunk_hash_table.glsl").string());
    clear_chunk_hash_table_cs = ComputeShader((p / "shaders" / "voxel_grid" / "clear_chunk_hash_table.glsl").string());
    voxel_mesh_vs = VertexShader((p / "shaders" / "voxel_grid" / "voxel_mesh.vert").string());
    voxel_mesh_fs = FragmentShader((p / "shaders" / "voxel_grid" / "voxel_mesh.frag").string());
}

