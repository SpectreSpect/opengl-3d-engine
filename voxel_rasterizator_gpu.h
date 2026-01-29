#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <vector>
#include <stdexcept>
#include <iostream>

#include "gridable.h"
#include "mesh_data.h"
#include "vertex_layout.h"
#include "ssbo.h"
#include "compute_shader.h"
#include "compute_program.h"

// GPU CSR: плотная ROI (Nx*Ny*Nz чанков)
class VoxelRasterizatorGPU {
public:
    Gridable* gridable = nullptr;

    VoxelRasterizatorGPU(
        Gridable* gridable, 
        ComputeShader* k_count_cs, 
        ComputeShader* k_scan_blocks_cs,
        ComputeShader* k_add_block_offsets_cs,
        ComputeShader* k_fix_last_cs,
        ComputeShader* k_copy_offsets_to_cursor_cs,
        ComputeShader* k_fill_cs,
        ComputeShader* k_voxelize_cs,
        ComputeShader* k_clear_cs
    );
    ~VoxelRasterizatorGPU();

    static inline int floor_div_int(int a, int b) {
        // b > 0
        int q = a / b;      // trunc toward 0
        int r = a % b;
        if (r != 0 && a < 0) --q; // корректируем до floor для отрицательных
        return q;
    }

    // ROI в координатах ЧАНКОВ (dense grid)
    void set_roi(glm::ivec3 chunk_origin, glm::uvec3 grid_dim);

    // ПОТОМ ПЕРЕВЕСТИ НА GPU!!!
    void calculate_roi_on_cpu(const MeshData& mesh_data,
                              const glm::mat4& transform,
                              const VertexLayout& vertex_layout,
                              float voxel_size,
                              int chunk_size,
                              int pad_voxels = 0);

    // Основная функция: построить CSR на GPU для mesh_data в ROI.
    // voxel_size: размер вокселя в world units
    // chunk_size: размер чанка в ВОКСЕЛЯХ по стороне (например 16)
    void rasterize(const MeshData& mesh_data,
                   const glm::mat4& transform,
                   const VertexLayout& vertex_layout,
                   float voxel_size,
                   int chunk_size);

    // Полезно для дебага/аллоков
    uint32_t last_total_pairs() const { return last_total_pairs_; }
    uint32_t chunk_count() const { return chunk_count_; }

private:
    // ROI
    glm::ivec3 roi_origin_{0,0,0};
    glm::uvec3 roi_dim_{1,1,1};

    // Programs
    ComputeProgram prog_count_;
    ComputeProgram prog_scan_blocks_;
    ComputeProgram prog_add_block_offsets_;
    ComputeProgram prog_fix_last_;
    ComputeProgram prog_copy_offsets_to_cursor_;
    ComputeProgram prog_fill_;
    ComputeProgram prog_voxelize_;
    ComputeProgram prog_clear_;

    // GPU buffers
    SSBO positions_ssbo_;       // vec3 positions (model space)
    SSBO tris_ssbo_;            // uvec3 (i0,i1,i2) per triangle
    SSBO counters_ssbo_;        // uint counters[chunkCount]
    SSBO offsets_ssbo_;         // uint offsets[chunkCount+1]
    SSBO cursor_ssbo_;          // uint cursor[chunkCount]
    SSBO tri_indices_ssbo_;     // uint triId[totalPairs]
    SSBO total_pairs_ssbo_;     // uint totalPairs (1 элемент)
    SSBO block_sums_ssbo_;      // uint blockSums[numBlocks]
    SSBO block_prefix_ssbo_;    // uint blockPrefix[numBlocks]
    SSBO colors_ssbo_;          // vec4 colors per vertex (rgb + pad)
    SSBO voxels_ssbo_;          // uint packed RGBA8 per voxel in ROI
    SSBO active_chunks_ssbo_;

    SSBO debug_ssbo_; // int dbg[32]

    // capacities (bytes)
    size_t pos_cap_bytes_ = 0;
    size_t tri_cap_bytes_ = 0;
    size_t counters_cap_bytes_ = 0;
    size_t offsets_cap_bytes_ = 0;
    size_t cursor_cap_bytes_ = 0;
    size_t tri_indices_cap_bytes_ = 0;
    size_t block_cap_bytes_ = 0;
    size_t col_cap_bytes_ = 0;
    size_t vox_cap_bytes_ = 0;
    size_t active_cap_bytes_ = 0;

    uint32_t last_total_pairs_ = 0;
    uint32_t chunk_count_ = 1;

    // scan scratch уровни (чтобы не было ограничения numBlocks<=256)
    std::vector<SSBO> scan_sums_;
    std::vector<SSBO> scan_prefix_;
    std::vector<size_t> scan_caps_; // bytes per level

private:
    void ensure_roi_buffers(size_t vertex_count, size_t tri_count, uint32_t chunk_count, uint32_t chunk_voxel_count);
    void ensure_active_chunk_buffers(uint32_t chunk_voxel_count, uint32_t pair_capacity, uint32_t activeCount);

    void ensure_scan_level(uint32_t level, uint32_t numBlocks);
    void gpu_exclusive_scan_u32_impl(SSBO& in_u32, SSBO& out_u32, uint32_t n, uint32_t level);

    void upload_positions_tris_colors(const MeshData& mesh_data, const VertexLayout& layout);

    // GPU exclusive scan: in=counters, out=offsets (первые n элементов), блок-суммы в scratch
    void gpu_exclusive_scan_u32(SSBO& in_u32, SSBO& out_u32, uint32_t n);

    // helpers
    static uint32_t div_up_u32(uint32_t a, uint32_t b) { return (a + b - 1u) / b; }
    uint32_t roi_chunk_count() const {
        return roi_dim_.x * roi_dim_.y * roi_dim_.z;
    }
};
