#include "shader_manager.h"

ShaderManager::ShaderManager(std::string& root_path) {
    init_shaders(root_path);
}

void ShaderManager::init_shaders(std::string& root_path) {
    fs::path p(root_path);
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

}

