#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) coherent buffer ChunkHashKeys { uvec2 hash_keys[]; };
layout(std430, binding=1) coherent buffer ChunkHashVals { uint  hash_vals[]; };
layout(std430, binding=2) buffer StreamCounters { uvec2 stream; }; // x=loadCount
layout(std430, binding=3) readonly buffer LoadList { uint load_list[]; };
layout(std430, binding=4) writeonly restrict buffer ChunkVoxels { VoxelData voxels[]; };
layout(std430, binding=5) buffer ChunkMetaBuf { ChunkMeta meta[]; };
layout(std430, binding=6) buffer EnqueuedBuf { uint enqueued[]; };
layout(std430, binding=7) buffer DirtyListBuf { uint dirty_count; uint dirty_list[]; };

uniform ivec3 u_chunk_dim;
uniform uint  u_voxels_per_chunk;

uniform uint u_pack_bits;
uniform int  u_pack_offset;

uniform uint u_set_dirty_flag_bits; // 1u
uniform uint u_seed;

uniform uint u_hash_table_size;

// ----- include -----
#include "common/common.glsl"

#define NOT_INCLUDE_GET_OR_CREATE
#include "common/hash_table.glsl"
// -------------------


// ---- helpers ----
uint voxel_index(ivec3 p) {
    return uint((p.z * u_chunk_dim.y + p.y) * u_chunk_dim.x + p.x);
}

// ---- noise (value noise + fbm) ----
float valueNoise(vec2 x) {
    ivec2 i = ivec2(floor(x));
    vec2  f = fract(x);
    vec2  u = f*f*(3.0 - 2.0*f);

    float a = hash_ivec2(i + ivec2(0,0), u_seed);
    float b = hash_ivec2(i + ivec2(1,0), u_seed);
    float c = hash_ivec2(i + ivec2(0,1), u_seed);
    float d = hash_ivec2(i + ivec2(1,1), u_seed);

    return mix(mix(a,b,u.x), mix(c,d,u.x), u.y);
}

float fbm(vec2 p) {
    float s = 0.0;
    float a = 0.5;
    for (int o=0; o<5; ++o) {
        s += a * valueNoise(p);
        p *= 2.0;
        a *= 0.5;
    }
    return s;
}

void mark_dirty(uint chunkId) {
    uint was = atomicCompSwap(enqueued[chunkId], 0u, 1u);
    if (was == 0u) {
        atomicOr(meta[chunkId].dirty_flags, u_set_dirty_flag_bits);
        uint di = atomicAdd(dirty_count, 1u);
        dirty_list[di] = chunkId;
    }
}

void try_mark_neighbor(ivec3 ncoord) {
    uint id = lookup_chunk(pack_key_uvec2(ncoord, u_pack_offset, u_pack_bits));
    if (id == INVALID_ID) return;

    // (опционально) "атомарное чтение", чтобы избежать странностей кеша
    uint used = atomicAdd(meta[id].used, 0u);
    if (used == 1u) {
        mark_dirty(id);
    }
}

void main() {
    uint voxelId = gl_GlobalInvocationID.x;
    uint listIdx = gl_GlobalInvocationID.y;

    uint loadCount = stream.x;
    if (listIdx >= loadCount) return;
    if (voxelId >= u_voxels_per_chunk) return;

    uint chunkId = load_list[listIdx];

    ivec3 chunkCoord = unpack_key_to_coord(uvec2(meta[chunkId].key_lo, meta[chunkId].key_hi), u_pack_offset, u_pack_bits);

    int lx = int(voxelId % uint(u_chunk_dim.x));
    int ly = int((voxelId / uint(u_chunk_dim.x)) % uint(u_chunk_dim.y));
    int lz = int(voxelId / uint(u_chunk_dim.x * u_chunk_dim.y));
    ivec3 local = ivec3(lx, ly, lz);

    ivec3 worldVoxel = chunkCoord * u_chunk_dim + local;

    // // ---- terrain height from fbm(xz) ----
    vec2 xz = vec2(worldVoxel.x, worldVoxel.z) * 0.03; // частота
    float n = fbm(xz); // 0..~1
    // float n = 0.5f;
    float height = 20.0 + n * 30.0; // базовый уровень + амплитуда

    uint type = (float(worldVoxel.y) <= height) ? 1u : 0u;
    uint vis  = (type != 0u) ? 1u : 0u;

    VoxelData vd;
    vd.type_vis_flags = (type << TYPE_SHIFT) | (vis << VIS_SHIFT);
    // цвет: чуть меняем по высоте
    vec3 col = (type != 0u) ? mix(vec3(0.15,0.35,0.10), vec3(0.45,0.30,0.15), n) : vec3(0.0);
    vd.color = pack_color(col);

    uint base = chunkId * u_voxels_per_chunk;
    voxels[base + voxelId] = vd;

    // один раз на чанк
    if (voxelId == 0u) {
        mark_dirty(chunkId); // Заставляем перестроить меш у себя

        // А также у всех чанков вокруг
        try_mark_neighbor(chunkCoord + ivec3( 1, 0, 0));
        try_mark_neighbor(chunkCoord + ivec3(-1, 0, 0));
        try_mark_neighbor(chunkCoord + ivec3( 0, 1, 0));
        try_mark_neighbor(chunkCoord + ivec3( 0,-1, 0));
        try_mark_neighbor(chunkCoord + ivec3( 0, 0, 1));
        try_mark_neighbor(chunkCoord + ivec3( 0, 0,-1));
    }
}
