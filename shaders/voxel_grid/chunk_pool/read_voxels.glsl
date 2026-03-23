#pragma once
#include "../../utils.glsl"
#include "voxel_index.glsl"
#include "../chunk_hash_table/lookup_remove.glsl"


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
    uint slot_id = chunk_hash_table_lookup_hash_table_slot_id(key);
    if (slot_id == INVALID_ID) return 0u;

    uint chunk_id = chunk_hash_table_slots[slot_id].value;
    return voxel_type_in_chunk(chunk_id, local);
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
