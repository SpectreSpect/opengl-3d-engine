#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) coherent buffer ChunkHashKeys { uvec2 hash_keys[]; };
layout(std430, binding=1) coherent buffer ChunkHashVals { uint  hash_vals[]; };
layout(std430, binding=2) readonly buffer VoxelWriteList { uint write_count; VoxelWrite writes[]; };
layout(std430, binding=3) buffer ChunkVoxels { VoxelData voxels[]; };
layout(std430, binding=4) buffer FreeList { uint free_count; uint free_list[]; };
layout(std430, binding=5) buffer ChunkMetaBuf { ChunkMeta meta[]; };
layout(std430, binding=6) buffer EnqueuedBuf { uint enqueued[]; };
layout(std430, binding=7) buffer DirtyListBuf { uint dirty_count; uint dirty_list[]; };

uniform uint  u_hash_table_size;
uniform ivec3 u_chunk_dim;
uniform uint  u_voxels_per_chunk;
uniform uint  u_set_dirty_flag_bits;

uniform uint u_pack_bits;
uniform int  u_pack_offset;

// ----- include -----
#include "../utils.glsl"
#include "common/hash_table.glsl"
// -------------------

// ---------- helpers ----------
uint voxel_index_in_chunk(ivec3 local) {
    return uint((local.z * u_chunk_dim.y + local.y) * u_chunk_dim.x + local.x);
}

void mark_dirty(uint chunkId) {
    atomicOr(meta[chunkId].dirty_flags, u_set_dirty_flag_bits);

    uint was = atomicExchange(enqueued[chunkId], 1u);
    if (was == 0u) {
        uint di = atomicAdd(dirty_count, 1u);
        dirty_list[di] = chunkId;
    }
}

void main() {
    uint wi = gl_GlobalInvocationID.x;
    uint writeCount = write_count;
    if (wi >= writeCount) return;

    VoxelWrite w = writes[wi];

    uint type = (w.voxel_data.type_vis_flags >> TYPE_SHIFT) & TYPE_MASK;
    if (type == 0u) return;

    ivec3 wc = w.world_voxel.xyz;

    ivec3 chunkCoord = ivec3(
        floor_div(wc.x, u_chunk_dim.x),
        floor_div(wc.y, u_chunk_dim.y),
        floor_div(wc.z, u_chunk_dim.z)
    );

    ivec3 local = ivec3(
        floor_mod(wc.x, u_chunk_dim.x),
        floor_mod(wc.y, u_chunk_dim.y),
        floor_mod(wc.z, u_chunk_dim.z)
    );

    uvec2 key = pack_key_uvec2(chunkCoord, u_pack_offset, u_pack_bits);

    uint chunkId;
    bool created;

    if (!get_or_create_chunk(key, chunkId, created)) return;
    if (chunkId == INVALID_ID) return;

    uint vi = voxel_index_in_chunk(local);

    // просто пишем VoxelData как есть (8 байт)
    voxels[chunkId * u_voxels_per_chunk + vi] = w.voxel_data;

    mark_dirty(chunkId);
}
