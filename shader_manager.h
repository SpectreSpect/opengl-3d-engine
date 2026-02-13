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
    // default shaders
    VertexShader default_vertex_shader;
    FragmentShader default_fragment_shader;

    VertexShader default_line_vertex_shader;
    FragmentShader default_line_fragment_shader;

    VertexShader default_point_vertex_shader;
    FragmentShader default_point_fragment_shader;

    // std::string default_vertex_shader_path = (executable_dir() / "shaders" / "deafult_vertex.glsl").string();
    // std::string default_fragment_shader_path = (executable_dir() / "shaders" / "deafult_fragment.glsl").string();

    // VertexShader* default_vertex_shader;
    // FragmentShader* default_fragment_shader;
    // VfProgram* default_program;


    // std::string default_line_vertex_shader_path = (executable_dir() / "shaders" / "line_vs.glsl").string();
    // std::string default_line_fragment_shader_path = (executable_dir() / "shaders" / "line_fs.glsl").string();

    // VertexShader* default_line_vertex_shader;
    // FragmentShader* default_line_fragment_shader;
    // Program* default_line_program;


    // std::string default_point_vertex_shader_path = (executable_dir() / "shaders" / "point_vs.glsl").string();
    // std::string default_point_fragment_shader_path = (executable_dir() / "shaders" / "point_fs.glsl").string();

    // VertexShader* default_point_vertex_shader;
    // FragmentShader* default_point_fragment_shader;
    // Program* default_point_program;

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
    VertexShader voxel_mesh_vs;
    FragmentShader voxel_mesh_fs;

    std::vector<Shader*> shaders;

    ShaderManager() = default;
    ShaderManager(const std::string& root_path);
    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;
    ShaderManager(ShaderManager&&) = delete;
    ShaderManager& operator=(ShaderManager&&) = delete;

    template<class T, class U>
    inline void add_shader(T& dst, U&& shader) {
        dst = T(std::forward<U>(shader));
        shaders.push_back(&dst);
    }

    void init_shaders(const std::string& root_path);
};