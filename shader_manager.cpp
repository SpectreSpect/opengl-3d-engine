#include "shader_manager.h"

ShaderManager::ShaderManager(const std::string& root_path) {
    init_shaders(root_path);
}

// ShaderManager::ShaderManager(ShaderManager&& o) noexcept {
//     shaders = std::move(o.shaders);
// }

void ShaderManager::init_shaders(const std::string& root_path) {
    fs::path p(root_path);

    add_shader(default_vertex_shader, VertexShader((p / "shaders" / "deafult_vertex.glsl").string())); 
    add_shader(default_fragment_shader, FragmentShader((p / "shaders" / "deafult_fragment.glsl").string())); 

    add_shader(default_line_vertex_shader, VertexShader((p / "shaders" / "line_vs.glsl").string())); 
    add_shader(default_line_fragment_shader, FragmentShader((p / "shaders" / "line_fs.glsl").string())); 

    add_shader(default_point_vertex_shader, VertexShader((p / "shaders" / "point_vs.glsl").string())); 
    add_shader(default_point_fragment_shader, FragmentShader((p / "shaders" / "point_fs.glsl").string())); 

    add_shader(count_cs, ComputeShader((p / "shaders" / "voxel_rasterization" / "count_cs.glsl").string())); 
    add_shader(scan_blocks_cs, ComputeShader((p / "shaders" / "voxel_rasterization" / "scan_blocks_cs.glsl").string()));
    add_shader(add_block_offsets_cs, ComputeShader((p / "shaders" / "voxel_rasterization" / "add_block_offsets_cs.glsl").string()));
    add_shader(fix_last_cs, ComputeShader((p / "shaders" / "voxel_rasterization" / "fix_last_cs.glsl").string()));
    add_shader(copy_offsets_to_cursor_cs, ComputeShader((p / "shaders" / "voxel_rasterization" / "copy_offsets_to_cursor_cs.glsl").string()));
    add_shader(fill_cs, ComputeShader((p / "shaders" / "voxel_rasterization" / "fill_cs.glsl").string()));
    add_shader(voxelize_cs, ComputeShader((p / "shaders" / "voxel_rasterization" / "voxelize_cs.glsl").string()));
    add_shader(clear_cs, ComputeShader((p / "shaders" / "voxel_rasterization" / "clear_cs.glsl").string()));
    add_shader(roi_reduce_indices_cs, ComputeShader((p / "shaders" / "voxel_rasterization" / "roi_reduce_indices_cs.glsl").string()));
    add_shader(roi_reduce_pairs_cs, ComputeShader((p / "shaders" / "voxel_rasterization" / "roi_reduce_pairs_cs.glsl").string()));
    add_shader(build_active_chunks_cs, ComputeShader((p / "shaders" / "voxel_rasterization" / "build_active_chunks_cs.glsl").string()));
    add_shader(roi_finalize_cs, ComputeShader((p / "shaders" / "voxel_rasterization" / "roi_finalize_cs.glsl").string()));

    add_shader(clear_chunks_cs, ComputeShader((p / "shaders" / "voxel_grid" / "clear_chunks.glsl").string()));
    add_shader(set_chunks_cs, ComputeShader((p / "shaders" / "voxel_grid" / "set_chunks.glsl").string()));
    add_shader(world_init_cs, ComputeShader((p / "shaders" / "voxel_grid" / "world_init.glsl").string()));
    add_shader(apply_writes_to_world_cs, ComputeShader((p / "shaders" / "voxel_grid" / "apply_writes_to_world.glsl").string()));
    add_shader(mesh_counters_reset_cs, ComputeShader((p / "shaders" / "voxel_grid" / "mesh_counters_reset.glsl").string()));
    add_shader(mesh_reset_cs, ComputeShader((p / "shaders" / "voxel_grid" / "mesh_reset.glsl").string()));
    add_shader(mesh_count_cs, ComputeShader((p / "shaders" / "voxel_grid" / "mesh_count.glsl").string()));
    add_shader(mesh_alloc_cs, ComputeShader((p / "shaders" / "voxel_grid" / "mesh_alloc.glsl").string()));
    add_shader(mesh_emit_cs, ComputeShader((p / "shaders" / "voxel_grid" / "mesh_emit.glsl").string()));
    add_shader(mesh_finalize_cs, ComputeShader((p / "shaders" / "voxel_grid" / "mesh_finalize.glsl").string()));
    add_shader(cmdcount_reset_cs, ComputeShader((p / "shaders" / "voxel_grid" / "cmdcount_reset.glsl").string()));
    add_shader(build_indirect_cmds_cs, ComputeShader((p / "shaders" / "voxel_grid" / "build_indirect_cmds.glsl").string()));
    add_shader(reset_dirty_count_cs, ComputeShader((p / "shaders" / "voxel_grid" / "reset_dirty_count.glsl").string()));
    add_shader(bucket_reset_cs, ComputeShader((p / "shaders" / "voxel_grid" / "evict_buckets_reset.glsl").string()));
    add_shader(bucket_build_cs, ComputeShader((p / "shaders" / "voxel_grid" / "evict_buckets_build.glsl").string()));
    add_shader(evict_lowprio_cs, ComputeShader((p / "shaders" / "voxel_grid" / "evict_low_priority.glsl").string()));
    add_shader(stream_select_chunks_cs, ComputeShader((p / "shaders" / "voxel_grid" / "stream_select_chunks.glsl").string()));
    add_shader(stream_generate_terrain_cs, ComputeShader((p / "shaders" / "voxel_grid" / "stream_generate_terrain.glsl").string()));
    add_shader(mark_all_user_chunks_as_dirty_cs, ComputeShader((p / "shaders" / "voxel_grid" / "mark_all_user_chunks_as_dirty.glsl").string()));
    add_shader(voxel_mesh_vs, VertexShader((p / "shaders" / "voxel_grid" / "voxel_mesh.vert").string()));
    add_shader(voxel_mesh_fs, FragmentShader((p / "shaders" / "voxel_grid" / "voxel_mesh.frag").string()));
    
}

