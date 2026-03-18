#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) coherent buffer ChunkHashKeys { uvec2 hash_keys[]; };
layout(std430, binding=1) coherent buffer ChunkHashVals { uint count_tomb; uint  hash_vals[]; };
layout(std430, binding=2) buffer FreeList { uint free_count; uint free_list[]; };
layout(std430, binding=3) buffer ChunkMetaBuf { ChunkMeta meta[]; };
layout(std430, binding=4) buffer EnqueuedBuf { uint enqueued[]; };
layout(std430, binding=5) buffer DirtyListBuf { uint dirty_count; uint dirty_list[]; };

layout(std430, binding=6) readonly buffer VoxelsWriteData { uint count_voxel_writes; uint pad_[3u]; VoxelWrite voxel_writes[]; };
layout(std430, binding=7) buffer ChunkVoxels { VoxelData voxels[]; };

uniform uint u_hash_table_size;
uniform ivec3 u_chunk_dim;
uniform uint u_voxels_per_chunk;

uniform uint u_pack_offset;
uniform uint u_pack_bits;

// ----- include -----
#include "../utils.glsl"
#include "common/hash_table.glsl"

#include "common/chunk_pool.glsl"
// -------------------

void main() {
    uint voxel_write_id = gl_GlobalInvocationID.x;
    if (voxel_write_id >= count_voxel_writes) return;

    VoxelWrite voxel_write = voxel_writes[voxel_write_id];
    ivec3 chunk_pos = floor_div(voxel_write.world_voxel.xyz, u_chunk_dim);
    ivec3 local_voxel_pos = voxel_write.world_voxel.xyz - chunk_pos * u_chunk_dim;
    
    uvec2 key = pack_key_uvec2(chunk_pos, u_pack_offset, u_pack_bits);

    uint chunk_id;
    bool created;
    if (!get_or_create_chunk(key, chunk_id, created)) return;
    
    uint voxel_id = voxel_index(local_voxel_pos);
    uint global_voxel_id = chunk_id * u_voxels_per_chunk + voxel_id;
    
    if ((voxel_write.set_flags & OVERWRITE_BIT) > 0u) {
        voxels[global_voxel_id] = voxel_write.voxel_data;

        mark_dirty_around(chunk_id, chunk_pos);
    }
    else {
        uint voxel_visability = (voxels[global_voxel_id].type_vis_flags >> VIS_SHIFT) & VIS_MASK;
        if (voxel_visability == 0u) {
            voxels[global_voxel_id] = voxel_write.voxel_data;
            mark_dirty_around(chunk_id, chunk_pos);
        }
    }
}