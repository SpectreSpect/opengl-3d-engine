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
#include "math_utils.h"

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

    std::pair<std::vector<glm::vec4>, std::vector<glm::uvec4>> upload_positions_tris_colors(const MeshData& mesh_data, const VertexLayout& layout);

    // GPU exclusive scan: in=counters, out=offsets (первые n элементов), блок-суммы в scratch
    void gpu_exclusive_scan_u32(SSBO& in_u32, SSBO& out_u32, uint32_t n);

    // helpers
    static uint32_t div_up_u32(uint32_t a, uint32_t b) { return (a + b - 1u) / b; }
    uint32_t roi_chunk_count() const {
        return roi_dim_.x * roi_dim_.y * roi_dim_.z;
    }


    static inline int floor_div_pos_int(int a, int b) {
    // b > 0
    int q = a / b;
    int r = a % b;
    if (r < 0) q -= 1;
    return q;
}

    static inline bool in_roi_cpu(const glm::ivec3& c,
                                const glm::ivec3& roi_origin,
                                const glm::uvec3& roi_dim,
                                uint32_t& out_idx)
    {
        glm::ivec3 rel = c - roi_origin;

        // максимально "железная" проверка (никаких uint кастов до проверки)
        if (rel.x < 0 || rel.y < 0 || rel.z < 0) return false;
        if (rel.x >= (int)roi_dim.x || rel.y >= (int)roi_dim.y || rel.z >= (int)roi_dim.z) return false;

        out_idx = (uint32_t(rel.z) * roi_dim.y + uint32_t(rel.y)) * roi_dim.x + uint32_t(rel.x);
        return true;
    }

    // positions: как в SSBO Positions (vec4, w=1)
    // tris:      как в SSBO Tris (uvec4, используем xyz)
    // counters:  размер = roi_dim.x * roi_dim.y * roi_dim.z
    static inline void count_cs_cpu_equiv(
        const std::vector<glm::vec4>& positions,
        const std::vector<glm::uvec4>& tris,
        const glm::mat4& transform,
        float voxel_size,
        int chunk_size,
        const glm::ivec3& roi_origin,
        const glm::uvec3& roi_dim,
        std::vector<uint32_t>& counters_out,
        float eps = 1e-4f
    ) {
        if (voxel_size <= 0.0f) throw std::invalid_argument("voxel_size must be > 0");
        if (chunk_size <= 0)    throw std::invalid_argument("chunk_size must be > 0");

        const uint64_t chunk_count_u64 =
            uint64_t(roi_dim.x) * uint64_t(roi_dim.y) * uint64_t(roi_dim.z);

        if (chunk_count_u64 == 0) {
            counters_out.clear();
            return;
        }
        if (chunk_count_u64 > uint64_t(std::numeric_limits<size_t>::max())) {
            throw std::runtime_error("roi_dim is too large for host vector");
        }

        counters_out.assign((size_t)chunk_count_u64, 0u);

        const uint32_t tri_count = (uint32_t)tris.size();
        const size_t vcount = positions.size();

        for (uint32_t tid = 0; tid < tri_count; ++tid) {
            const glm::uvec4 t = tris[tid];

            // как на GPU: индексы должны быть валидны
            if (t.x >= vcount || t.y >= vcount || t.z >= vcount) {
                // для дебага лучше падать — иначе просто "тихо" пропустишь кучу
                throw std::runtime_error("count_cs_cpu_equiv: triangle index out of range");
            }

            glm::vec3 p0 = glm::vec3(transform * positions[t.x]) / voxel_size;
            glm::vec3 p1 = glm::vec3(transform * positions[t.y]) / voxel_size;
            glm::vec3 p2 = glm::vec3(transform * positions[t.z]) / voxel_size;

            glm::vec3 mn = glm::min(p0, glm::min(p1, p2));
            glm::vec3 mx = glm::max(p0, glm::max(p1, p2));

            mn -= glm::vec3(eps);
            mx += glm::vec3(eps);

            // ВАЖНО: делаем ровно как в твоём count_cs.glsl
            glm::ivec3 vmin((int)std::floor(mn.x), (int)std::floor(mn.y), (int)std::floor(mn.z));
            glm::ivec3 vmax((int)std::floor(mx.x), (int)std::floor(mx.y), (int)std::floor(mx.z));
            vmax = glm::max(vmax, vmin);

            glm::ivec3 cmin(
                floor_div_pos_int(vmin.x, chunk_size),
                floor_div_pos_int(vmin.y, chunk_size),
                floor_div_pos_int(vmin.z, chunk_size)
            );
            glm::ivec3 cmax(
                floor_div_pos_int(vmax.x, chunk_size),
                floor_div_pos_int(vmax.y, chunk_size),
                floor_div_pos_int(vmax.z, chunk_size)
            );

            for (int cz = cmin.z; cz <= cmax.z; ++cz)
            for (int cy = cmin.y; cy <= cmax.y; ++cy)
            for (int cx = cmin.x; cx <= cmax.x; ++cx) {
                uint32_t idx = 0;
                if (!in_roi_cpu(glm::ivec3(cx, cy, cz), roi_origin, roi_dim, idx)) continue;
                counters_out[idx] += 1u;
            }
        }
    }
};
