#include "voxel_grid_gpu.h"

VoxelGridGPU::VoxelGridGPU(glm::ivec3 chunk_size, glm::vec3 voxel_size, uint32_t count_active_chunks, uint32_t chunk_hash_table_size) {
    this->chunk_size = chunk_size;
    this->voxel_size = voxel_size;
    this->count_active_chunks = count_active_chunks;
    this->chunk_hash_table_size = chunk_hash_table_size;

    chunk_meta = SSBO(sizeof(ChunkMeta) * count_active_chunks, GL_DYNAMIC_DRAW);
    chunk_hash_keys = SSBO(sizeof(int) * 3 * chunk_hash_table_size, GL_DYNAMIC_DRAW); //Пока координаты, но должен быть ключи. WARNING!!!
    chunk_hash_vals = SSBO(sizeof(uint32_t) * chunk_hash_table_size, GL_DYNAMIC_DRAW);
    free_list = SSBO(sizeof(uint32_t) * count_active_chunks, GL_DYNAMIC_DRAW);
    active_list = SSBO(sizeof(uint32_t) * count_active_chunks, GL_DYNAMIC_DRAW); 
    
    uint32_t frame_counters_u[4] = {
        0, //voxel_write_count
        0, //dirty_count
        0, //cmd_count
        count_active_chunks //free_count
    };
    frame_counters = SSBO(sizeof(frame_counters_u), GL_DYNAMIC_DRAW, &frame_counters_u);

    equeued = SSBO(sizeof(uint32_t) * count_active_chunks, GL_DYNAMIC_DRAW);
    dirty_list = SSBO(sizeof(uint32_t) * count_active_chunks, GL_DYNAMIC_DRAW);

    chunk_mesh_meta = SSBO(sizeof(ChunkMeshMeta) * count_active_chunks, GL_DYNAMIC_DRAW);
}

void VoxelGridGPU::init_active_chunks(glm::ivec3 chunk_size, uint32_t count_active_chunks, const VoxelData& init_voxel_prifab) {
    uint32_t chunk_volume = chunk_size.x * chunk_size.y * chunk_size.z;
    active_chunks = SSBO(sizeof(VoxelData) * chunk_volume * count_active_chunks, GL_DYNAMIC_DRAW);

    // glUniform1ui(glGetUniformLocation(prog_roi_reduce_indices_.id, "uIndexCount"), indexCount);
}