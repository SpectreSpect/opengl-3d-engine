#pragma once
#include "../utils.glsl"

#define NOT_INCLUDE_GET_OR_CREATE
#include "hash_table.glsl"
#undef NOT_INCLUDE_GET_OR_CREATE


/*
Параметры кода:
#define TOMB_CHECK_LIST_SIZE n (число n должно быть n = 2^i)

Параметры подключения:
#define NOT_INCLUDE_ALL
#define INCLUDE_VOXEL_INDEX/NOT_INCLUDE_VOXEL_INDEX:
    uniform ivec3 u_chunk_dim;

#define INCLUDE_VOXEL_FUNCS/NOT_INCLUDE_VOXEL_FUNCS:
    uniform uint u_hash_table_size;
    uniform ivec3 u_chunk_dim;
    uniform uint  u_voxels_per_chunk;

    coherent buffer ChunkHashKeys { uvec2 hash_keys[]; };
    coherent buffer ChunkHashVals { uint count_tomb; uint  hash_vals[]; };
    buffer FreeList { uint free_count; uint free_list[]; };
    buffer ChunkMetaBuf { ChunkMeta meta[]; };
    readonly buffer ChunkVoxels { VoxelData voxels[]; };

#define INCLUDE_MARK_DIRTY/NOT_INCLUDE_MARK_DIRTY:
    uniform uint u_pack_offset; 
    uniform uint u_pack_bits;

    buffer EnqueuedBuf { uint enqueued[]; };
    buffer DirtyListBuf { uint dirty_count; uint dirty_list[]; };
*/

#ifndef NOT_INCLUDE_ALL
#define INCLUDE_ALL
#endif

#if (defined(INCLUDE_ALL) || defined(INCLUDE_VOXEL_INDEX)) && !defined(NOT_INCLUDE_VOXEL_INDEX) && !defined(VOXEL_INDEX_INCLUDED)
#define VOXEL_INDEX_INCLUDED

uint voxel_index(ivec3 p) {
    return uint((p.z * u_chunk_dim.y + p.y) * u_chunk_dim.x + p.x);
}
#endif

#if (defined(INCLUDE_ALL) || defined(INCLUDE_READ_VOXEL_FUNCS)) && !defined(NOT_INCLUDE_READ_VOXEL_FUNCS) && !defined(READ_VOXEL_FUNCS_INCLUDED)
#define READ_VOXEL_FUNCS_INCLUDED

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
#endif


#if (defined(INCLUDE_ALL) || defined(INCLUDE_MARK_DIRTY)) && !defined(NOT_INCLUDE_MARK_DIRTY) && !defined(MARK_DIRTY_INCLUDED)
#define MARK_DIRTY_INCLUDED

void mark_dirty(uint chunkId) {
    uint was = atomicCompSwap(enqueued[chunkId], 0u, 1u);
    if (was == 0u) {
        atomicOr(meta[chunkId].dirty_flags, DIRTY_FLAG_BIT);
        uint di = atomicAdd(dirty_count, 1u);
        dirty_list[di] = chunkId;
    }
}

void try_mark_neighbor(ivec3 ncoord, bool read_only = false) {
    uint id = lookup_chunk(pack_key_uvec2(ncoord, u_pack_offset, u_pack_bits), read_only);
    if (id == INVALID_ID) return;

    // (опционально) "атомарное чтение", чтобы избежать странностей кеша
    uint used = atomicAdd(meta[id].used, 0u);
    if (used == 1u) {
        mark_dirty(id);
    }
}

void mark_dirty_around(uint chunkId, ivec3 chunk_coord) {
    mark_dirty(chunkId); // Заставляем перестроить меш у себя

    // А также у всех чанков вокруг
    try_mark_neighbor(chunk_coord + ivec3( 1, 0, 0));
    try_mark_neighbor(chunk_coord + ivec3(-1, 0, 0));
    try_mark_neighbor(chunk_coord + ivec3( 0, 1, 0));
    try_mark_neighbor(chunk_coord + ivec3( 0,-1, 0));
    try_mark_neighbor(chunk_coord + ivec3( 0, 0, 1));
    try_mark_neighbor(chunk_coord + ivec3( 0, 0,-1));
}
#endif

#ifdef INCLUDE_ALL
#undef INCLUDE_ALL
#endif

#ifdef NOT_INCLUDE_ALL
#undef NOT_INCLUDE_ALL
#endif

#ifdef INCLUDE_VOXEL_INDEX
#undef INCLUDE_VOXEL_INDEX
#endif

#ifdef NOT_INCLUDE_VOXEL_INDEX
#undef NOT_INCLUDE_VOXEL_INDEX
#endif

#ifdef INCLUDE_READ_VOXEL_FUNCS
#undef INCLUDE_READ_VOXEL_FUNCS
#endif

#ifdef NOT_INCLUDE_READ_VOXEL_FUNCS
#undef NOT_INCLUDE_READ_VOXEL_FUNCS
#endif

#ifdef INCLUDE_MARK_DIRTY
#undef INCLUDE_MARK_DIRTY
#endif

#ifdef NOT_INCLUDE_MARK_DIRTY
#undef NOT_INCLUDE_MARK_DIRTY
#endif