#pragma once
#include "../../utils.glsl"

#define NOT_INCLUDE_GET_OR_CREATE
#include "hash_table.glsl"

/*
Для настройки #include можно определить следующие дефайны (не обязательно):
#define TOMB_CHECK_LIST_SIZE n (число n должно быть n = 2^i)

Также необходимо наличие следующих данных (названия обязательно должны соответсвовать указанным).

Буфферы:
coherent buffer ChunkHashKeys { uvec2 hash_keys[]; };
coherent buffer ChunkHashVals { uint  hash_vals[]; };
buffer FreeList { uint free_list[]; }; (только если подключён get_or_create_chunk())
buffer ChunkMetaBuf { ChunkMeta meta[]; }; (только если подключён get_or_create_chunk())

readonly buffer ChunkVoxels { VoxelData voxels[]; };

Юниформы:
uniform uint u_hash_table_size;
uniform ivec3 u_chunk_dim;
uniform uint  u_voxels_per_chunk;
*/

uint voxel_index(ivec3 p) {
    return uint((p.z * u_chunk_dim.y + p.y) * u_chunk_dim.x + p.x);
}

uint voxel_type_in_chunk(uint chunkId, ivec3 p) {
    uint idx = chunkId * u_voxels_per_chunk + voxel_index(p);
    return (voxels[idx].type_vis_flags >> TYPE_SHIFT) & TYPE_MASK;
}

uint voxel_color_in_chunk(uint chunkId, ivec3 p) {
    uint idx = chunkId * u_voxels_per_chunk + voxel_index(p);
    return voxels[idx].color;
}

uint voxel_type_world(ivec3 chunkCoord, ivec3 local) {
    uvec2 key = pack_key_uvec2(chunkCoord, u_pack_offset, u_pack_bits);
    uint cid = lookup_chunk(key);
    if (cid == INVALID_ID) return 0u;
    return voxel_type_in_chunk(cid, local);
}

uint neighbor_type(uint chunkId, ivec3 chunkCoord, ivec3 p, ivec3 d) {
    ivec3 q = p + d;
    if (q.x >= 0 && q.x < u_chunk_dim.x &&
        q.y >= 0 && q.y < u_chunk_dim.y &&
        q.z >= 0 && q.z < u_chunk_dim.z) {
        return voxel_type_in_chunk(chunkId, q);
    }

    ivec3 nChunk = chunkCoord;
    ivec3 nLocal = q;

    if (nLocal.x < 0) { nChunk.x -= 1; nLocal.x += u_chunk_dim.x; }
    if (nLocal.x >= u_chunk_dim.x) { nChunk.x += 1; nLocal.x -= u_chunk_dim.x; }

    if (nLocal.y < 0) { nChunk.y -= 1; nLocal.y += u_chunk_dim.y; }
    if (nLocal.y >= u_chunk_dim.y) { nChunk.y += 1; nLocal.y -= u_chunk_dim.y; }

    if (nLocal.z < 0) { nChunk.z -= 1; nLocal.z += u_chunk_dim.z; }
    if (nLocal.z >= u_chunk_dim.z) { nChunk.z += 1; nLocal.z -= u_chunk_dim.z; }

    return voxel_type_world(nChunk, nLocal);
}

// 0/1 занятость
uint occ(uint chunkId, ivec3 chunkCoord, ivec3 p, ivec3 d) {
    return (neighbor_type(chunkId, chunkCoord, p, d) != 0u) ? 1u : 0u;
}