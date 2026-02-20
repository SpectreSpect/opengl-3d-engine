#version 430
layout(local_size_x = 256) in;

#define TYPE_SHIFT 16u
#define VIS_SHIFT  8u
#define TYPE_MASK  0xFFu

#define SLOT_EMPTY  0xFFFFFFFFu
#define SLOT_LOCKED 0xFFFFFFFEu
#define INVALID_ID  0xFFFFFFFFu
#define SLOT_TOMB   0xFFFFFFFDu
#define MAX_PROBES  128u
#define LOCK_SPINS  5u

layout(std430, binding=0) coherent buffer ChunkHashKeys { uvec2 hash_keys[]; };
layout(std430, binding=1) coherent buffer ChunkHashVals { uint  hash_vals[]; };

layout(std430, binding=16) buffer StreamCounters { uvec2 stream; }; // x=loadCount
layout(std430, binding=17) readonly buffer LoadList { uint load_list[]; };

struct VoxelData { uint type_vis_flags; uint color; };
layout(std430, binding=3) writeonly restrict buffer ChunkVoxels { VoxelData voxels[]; };

struct ChunkMeta { uint used; uint key_lo; uint key_hi; uint dirty_flags; };
layout(std430, binding=6) buffer ChunkMetaBuf { ChunkMeta meta[]; };

layout(std430, binding=7) buffer EnqueuedBuf { uint enqueued[]; };
layout(std430, binding=8) buffer DirtyListBuf { uint dirty_list[]; };

struct FrameCounters {uint write_count; uint dirty_count; uint cmd_count; uint free_count; uint failed_dirty_count; };
layout(std430, binding=5) buffer FrameCountersBuf { FrameCounters counters; }; // y=dirtyCount

layout(std430, binding=25) buffer DebugCounters { uint debug_counters[]; };

uniform ivec3 u_chunk_dim;
uniform uint  u_voxels_per_chunk;
uniform vec3  u_voxel_size;

uniform uint u_pack_bits;
uniform int  u_pack_offset;

uniform uint u_set_dirty_flag_bits; // 1u
uniform uint u_seed;

uniform uint u_hash_table_size;

uint read_hash_val(uint idx) {
    // атомарное "чтение" (0 не меняет значение), помогает с порядком/видимостью
    return atomicAdd(hash_vals[idx], 0u);
}

// ---- unpack key (без uint64) ----
uint mask_bits(uint bits) { return (bits >= 32u) ? 0xFFFFFFFFu : ((1u << bits) - 1u); }

uint extract_bits_uvec2(uvec2 k, uint shift, uint bits) {
    uint m = mask_bits(bits);
    if (bits == 0u) return 0u;

    if (shift < 32u) {
        uint lo_part = k.x >> shift;
        uint res = lo_part;
        uint rem = 32u - shift;
        if (rem < bits) res |= (k.y << rem);
        return res & m;
    } else {
        uint s = shift - 32u;
        return (k.y >> s) & m;
    }
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

// ---- helpers ----
uint voxel_index(ivec3 p) {
    return uint((p.z * u_chunk_dim.y + p.y) * u_chunk_dim.x + p.x);
}

void mark_dirty(uint chunkId) {
    uint was = atomicCompSwap(enqueued[chunkId], 0u, 1u);
    if (was == 0u) {
        atomicOr(meta[chunkId].dirty_flags, u_set_dirty_flag_bits);
        uint di = atomicAdd(counters.dirty_count, 1u);
        dirty_list[di] = chunkId;
    }
}

// ---- noise (value noise + fbm) ----
uint hash_u32(uint x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

float hash2(ivec2 p) {
    uint h = uint(p.x) * 374761393u
           + uint(p.y) * 668265263u
           + u_seed     * 2246822519u;

    h = hash_u32(h);
    return float(h) * (1.0 / 4294967295.0);
}

uint hash_uvec2(uvec2 v) {
    uint x = v.x * 1664525u + 1013904223u;
    uint y = v.y * 22695477u + 1u;
    uint h = x ^ (y + (x << 16) + (x >> 16));
    h ^= h >> 16; h *= 0x7feb352du;
    h ^= h >> 15; h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

float valueNoise(vec2 x) {
    ivec2 i = ivec2(floor(x));
    vec2  f = fract(x);
    vec2  u = f*f*(3.0 - 2.0*f);

    float a = hash2(i + ivec2(0,0));
    float b = hash2(i + ivec2(1,0));
    float c = hash2(i + ivec2(0,1));
    float d = hash2(i + ivec2(1,1));

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

uint pack_color(vec3 rgb) {
    rgb = clamp(rgb, 0.0, 1.0);
    uint r = uint(rgb.r * 255.0 + 0.5);
    uint g = uint(rgb.g * 255.0 + 0.5);
    uint b = uint(rgb.b * 255.0 + 0.5);
    return (r) | (g<<8) | (b<<16) | (0xFFu<<24);
}

uint lookup_chunk(uvec2 key) {
    uint mask = u_hash_table_size - 1u;
    uint idx  = hash_uvec2(key) & mask;

    for (uint visited = 0u; visited < MAX_PROBES; ++visited) {
        uint v = read_hash_val(idx);

        if (v == SLOT_LOCKED) {
            uint spins = 0u;
            while (spins++ < 1024u) {
                v = atomicAdd(hash_vals[idx], 0u);
                if (v != SLOT_LOCKED) break;
            }
            if (v == SLOT_LOCKED) return INVALID_ID; 
        }

        if (v == SLOT_EMPTY) return INVALID_ID;

        if (v == SLOT_TOMB) { idx = (idx + 1u) & mask; continue; }

        // v = chunkId, гарантируем видимость hash_keys
        memoryBarrierBuffer();
        if (all(equal(hash_keys[idx], key))) return v;

        idx = (idx + 1u) & mask;
    }

    return INVALID_ID;
}

void try_mark_neighbor(ivec3 ncoord) {
    uint id = lookup_chunk(pack_key_uvec2(ncoord));
    if (id == INVALID_ID) {
        // atomicAdd(debug_counters[0], 1u);
        return;
    }

    // (опционально) "атомарное чтение", чтобы избежать странностей кеша
    uint used = atomicAdd(meta[id].used, 0u);
    if (used == 1u) {
        mark_dirty(id);
        // atomicAdd(debug_counters[1], 1u);
    }
}

void main() {
    uint voxelId = gl_GlobalInvocationID.x;
    uint listIdx = gl_GlobalInvocationID.y;

    uint loadCount = stream.x;
    if (listIdx >= loadCount) return;
    if (voxelId >= u_voxels_per_chunk) return;

    uint chunkId = load_list[listIdx];

    ivec3 chunkCoord = unpack_key_to_coord(uvec2(meta[chunkId].key_lo, meta[chunkId].key_hi));

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
