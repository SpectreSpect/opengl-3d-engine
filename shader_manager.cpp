#include "shader_manager.h"

ShaderManager::ShaderManager(
    const std::filesystem::path& root_path, 
    const std::vector<std::filesystem::path>* include_directories,
    const std::filesystem::path* debug_root_path) 
{
    init_shaders(root_path, include_directories, debug_root_path);
}

void ShaderManager::init_shaders(
    const std::filesystem::path& root_path, 
    const std::vector<std::filesystem::path>* include_directories,
    const std::filesystem::path* debug_root_path)
{
    if (debug_root_path != nullptr) {
        try {
        std::filesystem::remove_all(*debug_root_path);
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Filesystem error: " << e.what() << '\n';
        }
    }

    const fs::path& p = root_path;
    const fs::path*& d = debug_root_path;
    const fs::path s;
    dispatch_adapter_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "dispatch_adapter.glsl", p, d, include_directories);
    clear_buffer_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "clear_buffer.glsl", p, d, include_directories);
    clear_buffer_prog = ComputeProgram(&clear_buffer_cs);

    // count_triangles_in_chunks_cs = ComputeShader(p / "shaders" / "voxel_rasterization" / "count_triangles_in_chunks.glsl", include_directories); 
    // scan_blocks_cs = ComputeShader(p / "shaders" / "voxel_rasterization" / "scan_blocks.glsl", include_directories);
    // add_block_offsets_cs = ComputeShader(p / "shaders" / "voxel_rasterization" / "add_block_offsets.glsl", include_directories);
    // fix_last_cs = ComputeShader(p / "shaders" / "voxel_rasterization" / "fix_last.glsl", include_directories);
    // copy_offsets_to_cursor_cs = ComputeShader(p / "shaders" / "voxel_rasterization" / "copy_offsets_to_cursor.glsl", include_directories);
    // roi_reduce_indices_cs = ComputeShader(p / "shaders" / "voxel_rasterization" / "roi_reduce_indices.glsl", include_directories);
    // roi_reduce_pairs_cs = ComputeShader(p / "shaders" / "voxel_rasterization" / "roi_reduce_pairs.glsl", include_directories);
    // build_active_chunks_cs = ComputeShader(p / "shaders" / "voxel_rasterization" / "build_active_chunks.glsl");
    // roi_finalize_cs = ComputeShader(p / "shaders" / "voxel_rasterization" / "roi_finalize.glsl", include_directories);
    // build_voxel_writes_cs = ComputeShader(p / "shaders" / "voxel_rasterization" / "build_voxel_writes.glsl", include_directories);

    //---------------------------------------------------

    mark_and_count_active_chunks_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_rasterization" / "mark_and_count_active_chunks.glsl", p, d, include_directories);
    alloc_active_chunk_triangles_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_rasterization" / "alloc_active_chunk_triangles.glsl", p, d, include_directories);
    // copy_counters_from_counter_hash_table_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_rasterization" / "copy_counters_from_counter_hash_table.glsl", p, d, include_directories);
    reset_voxelize_pipeline_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_rasterization" / "reset_voxelize_pipeline.glsl", p, d, include_directories);
    fill_triangle_indices_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_rasterization" / "fill_triangle_indices.glsl", p, d, include_directories);
    voxelize_triangles_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_rasterization" / "voxelize_triangles.glsl", p, d, include_directories);

    //---------------------------------------------------

    clear_chunks_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "clear_chunks.glsl", p, d, include_directories);
    world_init_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "world_init.glsl", p, d, include_directories);
    apply_writes_to_world_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "apply_writes_to_world.glsl", p, d, include_directories);
    mesh_reset_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "mesh_reset.glsl", p, d, include_directories);
    mesh_count_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "mesh_count.glsl", p, d, include_directories);
    mesh_alloc_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "mesh_alloc.glsl", p, d, include_directories);
    mesh_emit_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "mesh_emit.glsl", p, d, include_directories);
    mesh_finalize_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "mesh_finalize.glsl", p, d, include_directories);
    build_indirect_cmds_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "build_indirect_cmds.glsl", p, d, include_directories);
    reset_dirty_count_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "reset_dirty_count.glsl", p, d, include_directories);
    evict_buckets_build_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "evict_buckets_build.glsl", p, d, include_directories);
    evict_low_priority_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "evict_low_priority.glsl", p, d, include_directories);
    evict_low_priority_dispatch_adapter_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "evict_low_priority_dispatch_adapter.glsl", p, d, include_directories);
    stream_select_chunks_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "stream_select_chunks.glsl", p, d, include_directories);
    stream_generate_terrain_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "stream_generate_terrain.glsl", p, d, include_directories);
    mark_all_user_chunks_as_dirty_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "mark_all_user_chunks_as_dirty.glsl", p, d, include_directories);
    mesh_pool_clear_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "mesh_pool_clear.glsl", p, d, include_directories);
    mesh_pool_seed_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "mesh_pool_seed.glsl", p, d, include_directories);
    verify_mesh_allocation_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "verify_mesh_allocation.glsl", p, d, include_directories);
    return_free_alloc_nodes_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "return_free_alloc_nodes.glsl", p, d, include_directories);
    return_free_alloc_nodes_dispatch_adapter_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "return_free_alloc_nodes_dispatch_adapter.glsl", p, d, include_directories);
    free_evicted_chunks_mesh_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "free_evicted_chunks_mesh.glsl", p, d, include_directories);
    fill_chunk_hash_table_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "fill_chunk_hash_table.glsl", p, d, include_directories);
    clear_chunk_hash_table_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "clear_chunk_hash_table.glsl", p, d, include_directories);
    reset_evicted_list_and_buckets_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "reset_evicted_list_and_buckets.glsl", p, d, include_directories);
    hash_table_conditional_dispatch_adapter_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "hash_table_conditional_dispatch_adapter.glsl", p, d, include_directories);
    mark_write_chunks_to_generate_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "mark_write_chunks_to_generate.glsl", p, d, include_directories);
    write_voxels_to_grid_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "write_voxels_to_grid.glsl", p, d, include_directories);
    insert_elements_to_voxel_write_list_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "insert_elements_to_voxel_write_list.glsl", p, d, include_directories);
    add_voxel_write_list_counters_together_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "add_voxel_write_list_counters_together.glsl", p, d, include_directories);
    voxel_mesh_vs = Shader::build_shader<VertexShader>(s / "shaders" / "voxel_grid" / "voxel_mesh.vert", p, d, include_directories);
    voxel_mesh_fs = Shader::build_shader<FragmentShader>(s / "shaders" / "voxel_grid" / "voxel_mesh.frag", p, d, include_directories);
}
