#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <vector>
#include <stdexcept>
#include <iostream>

#include "gridable_gpu.h"
#include "mesh.h"
#include "mesh_data.h"
#include "vertex_layout.h"
#include "buffer_object.h"
#include "compute_shader.h"
#include "compute_program.h"
#include "math_utils.h"
#include "shader_manager.h"
#include "voxel_engine_gpu_structures.h"

// GPU CSR: плотная ROI (Nx*Ny*Nz чанков)
class VoxelRasterizatorGPU {
public:
    IGridable* gridable = nullptr;
    IGridableGPU* gridable_gpu = nullptr;
    glm::ivec3 chunk_size; 
    glm::vec3 voxel_size;

    VoxelRasterizatorGPU(IGridable* gridable, const glm::ivec3& chunk_size, const glm::vec3& voxel_size, ShaderManager& shader_manager);
    ~VoxelRasterizatorGPU();

    // ROI в координатах ЧАНКОВ (dense grid)
    void set_roi(glm::ivec3 chunk_origin, glm::uvec3 grid_dim);

    void rasterize(
        const Mesh& mesh,
        uint32_t voxel_visability,
        uint32_t voxel_type,
        glm::uvec3 voxel_color,
        uint32_t set_flags,
        const ComputeShader* custom_apply_prog = nullptr
    );

    // Полезно для дебага/аллоков
    uint32_t last_total_pairs() const { return last_total_pairs_; }
    uint32_t chunk_count() const { return chunk_count_; }

private:
    ShaderManager* shader_manager = nullptr;

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
    // ComputeProgram prog_clear_;
    ComputeProgram prog_roi_reduce_indices_;
    ComputeProgram prog_roi_reduce_pairs_;
    ComputeProgram prog_roi_finalize_;
    ComputeProgram prog_build_active_chunks_;
    ComputeProgram prog_build_voxel_writes_;


    // GPU buffers
    BufferObject counters_ssbo_;        // uint counters[chunkCount]
    BufferObject offsets_ssbo_;         // uint offsets[chunkCount+1]
    BufferObject cursor_ssbo_;          // uint cursor[chunkCount]
    BufferObject tri_indices_ssbo_;     // uint triId[totalPairs]
    BufferObject total_pairs_ssbo_;     // uint totalPairs (1 элемент)
    BufferObject block_sums_ssbo_;      // uint blockSums[numBlocks]
    BufferObject block_prefix_ssbo_;    // uint blockPrefix[numBlocks]
    BufferObject voxels_ssbo_;          // uint packed RGBA8 per voxel in ROI
    BufferObject roi_out_ssbo_;
    BufferObject active_chunks_ssbo_;
    BufferObject active_count_ssbo_;
    std::vector<BufferObject> roi_reduce_levels_;
    BufferObject voxel_writes;

    BufferObject debug_ssbo_; // int dbg[32]

    // capacities (bytes)
    size_t counters_cap_bytes_ = 0;
    size_t offsets_cap_bytes_ = 0;
    size_t cursor_cap_bytes_ = 0;
    size_t tri_indices_cap_bytes_ = 0;
    size_t block_cap_bytes_ = 0;
    size_t vox_cap_bytes_ = 0;
    size_t active_cap_bytes_ = 0;
    size_t roi_out_cap_bytes_ = 0;
    size_t active_count_cap_bytes_ = 0;
    std::vector<size_t> roi_reduce_caps_;

    uint32_t last_total_pairs_ = 0;
    uint32_t chunk_count_ = 1;

    // scan scratch уровни (чтобы не было ограничения numBlocks<=256)
    std::vector<std::unique_ptr<BufferObject>> scan_sums_;
    std::vector<std::unique_ptr<BufferObject>> scan_prefix_;
    std::vector<size_t> scan_caps_; // bytes per level

private:
    void calculate_roi(const Mesh& mesh, float voxel_size, int chunk_size, int pad_voxels);
    void clear_counters();
    void count_triangles_in_chunks(const Mesh& mesh, float voxel_size, int chunk_size, uint32_t tri_count); //pass 1
    void fill_triangle_indices(const Mesh& mesh, float voxel_size, int chunk_size, size_t tri_count); //pass 3
    void voxelize_chunks(const Mesh& mesh, float voxel_size, int chunk_size, uint32_t active_count, uint32_t tri_count);

    void pass2_build_offsets_and_active_gpu(uint32_t chunk_count);

    void ensure_roi_reduce_level(uint32_t level, uint32_t numPairs); 
    void ensure_active_index_capacity(uint32_t chunk_count);
    void ensure_roi_buffers(size_t vertex_count, size_t tri_count, uint32_t chunk_count, uint32_t chunk_voxel_count);
    void ensure_active_chunk_buffers(uint32_t chunk_voxel_count, uint32_t pair_capacity, uint32_t activeCount);

    void ensure_scan_level(uint32_t level, uint32_t numBlocks);
    void gpu_exclusive_scan_u32_impl(BufferObject& in_u32, BufferObject& out_u32, uint32_t n, uint32_t level);

    void gpu_exclusive_scan_u32(BufferObject& in_u32, BufferObject& out_u32, uint32_t n);
    void build_voxel_writes(
        uint32_t active_count,
        glm::ivec3 chunk_dim,
        uint32_t voxel_visability,
        uint32_t voxel_type,
        glm::uvec3 voxel_color,
        uint32_t set_flags
    );

    glm::ivec3 idx_to_chunk(uint32_t idx);
};
