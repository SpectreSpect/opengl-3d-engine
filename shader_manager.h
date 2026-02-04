#pragma once
#include <iostream>
#include <string>
#include <filesystem>

#include "compute_shader.h"

namespace fs = std::filesystem;

class ShaderManager {
public:
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

    std::vector<Shader*> shaders;

    ShaderManager(std::string& root_path);
    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;
    ShaderManager(ShaderManager&&) = delete;
    ShaderManager& operator=(ShaderManager&&) = delete;

    template<class T, class U>
    inline void add_shader(T& dst, U&& shader) {
        dst = T(std::forward<U>(shader));
        shaders.push_back(&dst);
    }

    void init_shaders(std::string& root_path);
};