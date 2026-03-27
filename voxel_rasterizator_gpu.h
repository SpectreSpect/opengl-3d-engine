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
#include "shader_helper.h"

// GPU CSR: плотная ROI (Nx*Ny*Nz чанков)
class VoxelRasterizatorGPU {
public:
    IGridable* gridable = nullptr;
    IGridableGPU* gridable_gpu = nullptr;
    ShaderManager* shader_manager = nullptr;
    ShaderHelper* shader_helper = nullptr;

    glm::ivec3 chunk_size; 
    glm::vec3 voxel_size;
    uint32_t counter_hash_table_size;
    uint32_t count_voxel_writes;

    struct VoxelRasterizatorDesc {
        glm::ivec3 chunk_size;
        glm::vec3 voxel_size;
        uint32_t counter_hash_table_size;
        uint32_t count_voxel_writes;
    };

    VoxelRasterizatorGPU(const VoxelRasterizatorDesc& desc, IGridable* gridable, ShaderManager* shader_manager, ShaderHelper* shader_helper);
    ~VoxelRasterizatorGPU();

    void rasterize(const Mesh& mesh, const VoxelWriteGPU& prifab, BufferObject* out_voxel_writes = nullptr);

private:
    // Programs
    ComputeProgram prog_count_;
    ComputeProgram prog_scan_blocks_;
    ComputeProgram prog_add_block_offsets_;
    ComputeProgram prog_fix_last_;
    ComputeProgram prog_copy_offsets_to_cursor_;
    ComputeProgram prog_fill_triangle_indices_;
    ComputeProgram prog_voxelize_triangles_;
    // ComputeProgram prog_clear_;
    ComputeProgram prog_roi_reduce_indices_;
    ComputeProgram prog_roi_reduce_pairs_;
    ComputeProgram prog_roi_finalize_;
    ComputeProgram prog_build_active_chunks_;
    ComputeProgram prog_build_voxel_writes_;
    ComputeProgram prog_mark_and_count_active_chunks_;
    ComputeProgram prog_alloc_active_chunk_triangles_;
    ComputeProgram prog_copy_counters_from_counter_hash_table_;
    ComputeProgram prog_reset_voxelize_pipeline_;

    // GPU buffers
    BufferObject dispatch_args;
    BufferObject counter_hash_table_;
    BufferObject triangle_indices_list_;     // uint triId[totalPairs]
    BufferObject active_chunk_keys_list_;
    BufferObject voxel_writes_;


    
    BufferObject counters_list_;        // uint counters[chunkCount]
    BufferObject offsets_list_;         // uint offsets[chunkCount+1]

    BufferObject cursor_ssbo_;          // uint cursor[chunkCount]
    
    BufferObject total_pairs_ssbo_;     // uint totalPairs (1 элемент)
    BufferObject block_sums_ssbo_;      // uint blockSums[numBlocks]
    BufferObject block_prefix_ssbo_;    // uint blockPrefix[numBlocks]
    BufferObject voxels_ssbo_;          // uint packed RGBA8 per voxel in ROI
    BufferObject active_chunks_ssbo_;
    BufferObject active_count_ssbo_;
    

    // scan scratch уровни (чтобы не было ограничения numBlocks<=256)
    std::vector<BufferObject> scan_sums_;
    std::vector<BufferObject> scan_prefix_;
private:
    void init_programs();

    void reset_voxelize_pipline(BufferObject& voxel_writes, bool reset_voxel_write_list = true);
    void mark_and_count_active_chunks(const Mesh& mesh);
    void alloc_active_chunk_triangles();
    void fill_triangle_indices(const Mesh& mesh);
    void voxelize_chunks(
        const BufferObject& dispatch_args,
        const Mesh& mesh,
        BufferObject& voxel_writes,
        uint32_t voxel_type_vis_flags,
        uint32_t voxel_color,
        uint32_t voxel_set_flags
    );
};
