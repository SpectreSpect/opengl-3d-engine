#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) readonly buffer VoxelsOccupancy { uint voxels_occupancy[]; };
layout(std430, binding=1) readonly buffer ActiveChunks { uint active_chunks[]; };
layout(std430, binding=2) buffer VoxelWritesBuf { uint count_voxel_writes; uint pad_[3u]; VoxelWrite voxel_writes[]; };

uniform ivec3 roi_origin;
uniform uvec3 roi_dim;

uniform uvec3 chunk_dim;
uniform uint u_chunk_voxel_count;
uniform uint u_active_chunk_count;

uniform uint voxel_visability;
uniform uint voxel_type;
uniform uvec3 voxel_color;
uniform uint set_flags;

// ----- include -----
#include "../utils.glsl"
// -------------------

void main() {
    uint local_voxel_id = gl_GlobalInvocationID.x;
    uint id_in_active_chunks_list = gl_GlobalInvocationID.y;

    if (local_voxel_id >= u_chunk_voxel_count) return;
    if (id_in_active_chunks_list >= u_active_chunk_count) return;

    uint occupancy_id = local_voxel_id + u_chunk_voxel_count * id_in_active_chunks_list;
    if (voxels_occupancy[occupancy_id] == 0u) return;

    uint voxel_write_id = atomicAdd(count_voxel_writes, 1u);

    uint chunk_id = active_chunks[id_in_active_chunks_list];

    // Определение позиции чанка внутри roi
    uint roi_chunk_x = chunk_id % roi_dim.x;
    
    uint roi_chunk_yz = chunk_id / roi_dim.x;
    uint roi_chunk_y = roi_chunk_yz % roi_dim.y;

    uint roi_chunk_z = roi_chunk_yz / roi_dim.y;

    // Позиция чанка
    ivec3 chunk_pos = roi_origin + ivec3(roi_chunk_x, roi_chunk_y, roi_chunk_z);

    // Определение позиции вокселя внутри чанка
    uint local_voxel_x = local_voxel_id % chunk_dim.x;
    
    uint local_voxel_yz = local_voxel_id / chunk_dim.x;
    uint local_voxel_y = local_voxel_yz % chunk_dim.y;

    uint local_voxel_z = local_voxel_yz / chunk_dim.y;

    // Глобальная позиция вокселя
    ivec3 voxel_pos = chunk_pos * ivec3(chunk_dim) + ivec3(local_voxel_x, local_voxel_y, local_voxel_z);

    // Заполнение данных вокселя
    VoxelWrite voxel_write;
    voxel_write.world_voxel = ivec4(voxel_pos, 0u);

    uint voxel_flags = voxel_visability == 1u ? (1u << VOXEL_VISABILITY_FLAG_BIT) : (1u << VOXEL_EASY_OVERWRITE_FLAG_BIT);
    voxel_write.voxel_data.type_flags = pack_voxel_type_flags(voxel_type, voxel_flags);
    voxel_write.voxel_data.color = pack_color(voxel_color);
    voxel_write.set_flags = set_flags;

    voxel_writes[voxel_write_id] = voxel_write;
}