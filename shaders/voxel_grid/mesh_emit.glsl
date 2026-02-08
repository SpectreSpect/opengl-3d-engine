#version 430
layout(local_size_x = 256) in;

#define SLOT_EMPTY  0xFFFFFFFFu
#define SLOT_LOCKED 0xFFFFFFFEu
#define INVALID_ID  0xFFFFFFFFu
#define MAX_PROBES  128u
#define LOCK_SPINS  5u

#define TYPE_SHIFT 16u
#define VIS_SHIFT  8u
#define TYPE_MASK  0xFFu

layout(std430, binding=0) coherent buffer ChunkHashKeys { uvec2 hash_keys[]; };
layout(std430, binding=1) coherent buffer ChunkHashVals { uint  hash_vals[]; };

// === NEW: соответствует VoxelDataGPU ===
struct VoxelData {
    uint type_vis_flags;
    uint color; // RGBA8
};
layout(std430, binding=3) readonly buffer ChunkVoxels { VoxelData voxels[]; };

layout(std430, binding=5) buffer FrameCounters { uvec4 counters; }; // y = dirtyCount
layout(std430, binding=8) readonly buffer DirtyListBuf { uint dirty_list[]; };

layout(std430, binding=12) buffer EmitCounterBuf { uint emit_counter[]; };

struct ChunkMeshMeta { uint first_index; uint index_count; uint base_vertex; uint mesh_valid; };
layout(std430, binding=9) readonly buffer ChunkMeshMetaBuf { ChunkMeshMeta mesh_meta[]; };

struct ChunkMeta { uint used; uint key_lo; uint key_hi; uint dirty_flags; };
layout(std430, binding=6) readonly buffer ChunkMetaBuf { ChunkMeta meta[]; };

// ===== Vertex / Index buffers =====
struct Vertex {
    vec4 pos;
    uint color;
    uint face;
    uint pad0;
    uint pad1;
};
layout(std430, binding=13) buffer GlobalVB { Vertex vb[]; };
layout(std430, binding=14) buffer GlobalIB { uint ib[]; };

// ===== uniforms =====
uniform uint  u_hash_table_size;
uniform ivec3 u_chunk_dim;
uniform uint  u_voxels_per_chunk;
uniform vec3  u_voxel_size;

uniform uint u_pack_bits;
uniform int  u_pack_offset;

// ===== hash + lookup =====
uint hash_uvec2(uvec2 v) {
    uint x = v.x * 1664525u + 1013904223u;
    uint y = v.y * 22695477u + 1u;
    uint h = x ^ (y + (x << 16) + (x >> 16));
    h ^= h >> 16; h *= 0x7feb352du;
    h ^= h >> 15; h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

uint read_hash_val(uint idx) {
    // атомарное "чтение" (0 не меняет значение), помогает с порядком/видимостью
    return atomicAdd(hash_vals[idx], 0u);
}

uint lookup_chunk(uvec2 key) {
    uint mask = u_hash_table_size - 1u;
    uint idx  = hash_uvec2(key) & mask;

    for (uint visited = 0u; visited < MAX_PROBES; ++visited) {
        uint v = read_hash_val(idx);

        if (v == SLOT_EMPTY) return INVALID_ID;

        if (v == SLOT_LOCKED) {
            // ждём и перепроверяем тот же idx, visited НЕ увеличиваем
            uint spins = 0u;
            while (spins++ < 1024u) { // можно поменьше/побольше; 5 реально мало при большом контеншне
                v = read_hash_val(idx);
                if (v != SLOT_LOCKED) break;
            }
            continue;
        }

        // v = chunkId, гарантируем видимость hash_keys
        memoryBarrierBuffer();
        if (all(equal(hash_keys[idx], key))) return v;

        idx = (idx + 1u) & mask;
    }

    return INVALID_ID;
}

// ===== pack/unpack uvec2 (без uint64) =====
uint mask_bits(uint bits) {
    return (bits >= 32u) ? 0xFFFFFFFFu : ((1u << bits) - 1u);
}

uint extract_bits_uvec2(uvec2 k, uint shift, uint bits) {
    uint m = mask_bits(bits);
    if (bits == 0u) return 0u;

    if (shift < 32u) {
        uint lo_part = k.x >> shift;
        uint res = lo_part;

        uint rem = 32u - shift;
        if (rem < bits) {
            uint hi_part = k.y << rem;
            res |= hi_part;
        }
        return res & m;
    } else {
        uint s = shift - 32u;
        uint res = k.y >> s;
        return res & m;
    }
}

uvec2 pack_key_uvec2(ivec3 c) {
    uint B = u_pack_bits;
    uint m = mask_bits(B);

    uint ux = uint(c.x + u_pack_offset) & m;
    uint uy = uint(c.y + u_pack_offset) & m;
    uint uz = uint(c.z + u_pack_offset) & m;

    uint lo = 0u;
    uint hi = 0u;

    lo |= uz;

    if (B < 32u) {
        lo |= (uy << B);
        if (B != 0u && (B + B) > 32u) {
            hi |= (uy >> (32u - B));
        }
    } else {
        hi |= (uy << (B - 32u));
    }

    uint s = 2u * B;
    if (s < 32u) {
        lo |= (ux << s);
        if (B != 0u && (s + B) > 32u) {
            hi |= (ux >> (32u - s));
        }
    } else if (s == 32u) {
        hi |= ux;
    } else {
        hi |= (ux << (s - 32u));
    }

    return uvec2(lo, hi);
}

ivec3 unpack_key_to_coord(uvec2 key2) {
    uint B = u_pack_bits;
    uint ux = extract_bits_uvec2(key2, 2u * B, B);
    uint uy = extract_bits_uvec2(key2, 1u * B, B);
    uint uz = extract_bits_uvec2(key2, 0u * B, B);

    return ivec3(int(ux) - u_pack_offset,
                 int(uy) - u_pack_offset,
                 int(uz) - u_pack_offset);
}

// ===== voxel helpers =====
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
    uvec2 key = pack_key_uvec2(chunkCoord);
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

// ===== emit quad =====
void emit_quad(uint chunkId, ivec3 chunkCoord, ivec3 p, uint face, uint color) {
    uint maxQuads = mesh_meta[chunkId].index_count / 6u;
    uint q = atomicAdd(emit_counter[chunkId], 1u);
    if (q >= maxQuads) return;

    uint baseV = mesh_meta[chunkId].base_vertex + q * 4u;
    uint baseI = mesh_meta[chunkId].first_index + q * 6u;

    vec3 wp = vec3(chunkCoord * u_chunk_dim + p) * u_voxel_size;

    vec3 v0; vec3 v1; vec3 v2; vec3 v3;

    if (face == 0u) { // +X
        v0 = wp + vec3(u_voxel_size.x, 0, 0);
        v1 = wp + vec3(u_voxel_size.x, u_voxel_size.y, 0);
        v2 = wp + vec3(u_voxel_size.x, u_voxel_size.y, u_voxel_size.z);
        v3 = wp + vec3(u_voxel_size.x, 0, u_voxel_size.z);
    } else if (face == 1u) { // -X
        v0 = wp + vec3(0, 0, 0);
        v1 = wp + vec3(0, 0, u_voxel_size.z);
        v2 = wp + vec3(0, u_voxel_size.y, u_voxel_size.z);
        v3 = wp + vec3(0, u_voxel_size.y, 0);
    } else if (face == 2u) { // +Y
        v0 = wp + vec3(0, u_voxel_size.y, 0);
        v1 = wp + vec3(0, u_voxel_size.y, u_voxel_size.z);
        v2 = wp + vec3(u_voxel_size.x, u_voxel_size.y, u_voxel_size.z);
        v3 = wp + vec3(u_voxel_size.x, u_voxel_size.y, 0);
    } else if (face == 3u) { // -Y
        v0 = wp + vec3(0, 0, 0);
        v1 = wp + vec3(u_voxel_size.x, 0, 0);
        v2 = wp + vec3(u_voxel_size.x, 0, u_voxel_size.z);
        v3 = wp + vec3(0, 0, u_voxel_size.z);
    } else if (face == 4u) { // +Z
        v0 = wp + vec3(0, 0, u_voxel_size.z);
        v1 = wp + vec3(u_voxel_size.x, 0, u_voxel_size.z);
        v2 = wp + vec3(u_voxel_size.x, u_voxel_size.y, u_voxel_size.z);
        v3 = wp + vec3(0, u_voxel_size.y, u_voxel_size.z);
    } else { // -Z
        v0 = wp + vec3(0, 0, 0);
        v1 = wp + vec3(0, u_voxel_size.y, 0);
        v2 = wp + vec3(u_voxel_size.x, u_voxel_size.y, 0);
        v3 = wp + vec3(u_voxel_size.x, 0, 0);
    }

    vb[baseV + 0u].pos = vec4(v0, 1.0); vb[baseV + 0u].color = color; vb[baseV + 0u].face = face;
    vb[baseV + 1u].pos = vec4(v1, 1.0); vb[baseV + 1u].color = color; vb[baseV + 1u].face = face;
    vb[baseV + 2u].pos = vec4(v2, 1.0); vb[baseV + 2u].color = color; vb[baseV + 2u].face = face;
    vb[baseV + 3u].pos = vec4(v3, 1.0); vb[baseV + 3u].color = color; vb[baseV + 3u].face = face;

    ib[baseI + 0u] = baseV + 0u;
    ib[baseI + 1u] = baseV + 1u;
    ib[baseI + 2u] = baseV + 2u;
    ib[baseI + 3u] = baseV + 0u;
    ib[baseI + 4u] = baseV + 2u;
    ib[baseI + 5u] = baseV + 3u;
}

void main() {
    uint voxelId  = gl_GlobalInvocationID.x;
    uint dirtyIdx = gl_GlobalInvocationID.y;

    uint dirtyCount = counters.y;
    if (dirtyIdx >= dirtyCount) return;
    if (voxelId >= u_voxels_per_chunk) return;

    uint chunkId = dirty_list[dirtyIdx];

    uvec2 key2 = uvec2(meta[chunkId].key_lo, meta[chunkId].key_hi);
    ivec3 chunkCoord = unpack_key_to_coord(key2);

    int lx = int(voxelId % uint(u_chunk_dim.x));
    int ly = int((voxelId / uint(u_chunk_dim.x)) % uint(u_chunk_dim.y));
    int lz = int(voxelId / uint(u_chunk_dim.x * u_chunk_dim.y));
    ivec3 p = ivec3(lx, ly, lz);

    uint t = voxel_type_in_chunk(chunkId, p);
    if (t == 0u) return;

    uint color = voxel_color_in_chunk(chunkId, p);

    if (neighbor_type(chunkId, chunkCoord, p, ivec3( 1, 0, 0)) == 0u) emit_quad(chunkId, chunkCoord, p, 0u, color);
    if (neighbor_type(chunkId, chunkCoord, p, ivec3(-1, 0, 0)) == 0u) emit_quad(chunkId, chunkCoord, p, 1u, color);
    if (neighbor_type(chunkId, chunkCoord, p, ivec3( 0, 1, 0)) == 0u) emit_quad(chunkId, chunkCoord, p, 2u, color);
    if (neighbor_type(chunkId, chunkCoord, p, ivec3( 0,-1, 0)) == 0u) emit_quad(chunkId, chunkCoord, p, 3u, color);
    if (neighbor_type(chunkId, chunkCoord, p, ivec3( 0, 0, 1)) == 0u) emit_quad(chunkId, chunkCoord, p, 4u, color);
    if (neighbor_type(chunkId, chunkCoord, p, ivec3( 0, 0,-1)) == 0u) emit_quad(chunkId, chunkCoord, p, 5u, color);
}
