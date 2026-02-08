#version 430
layout(local_size_x = 256) in;

#define SLOT_EMPTY   0xFFFFFFFFu
#define SLOT_LOCKED  0xFFFFFFFEu
#define INVALID_ID   0xFFFFFFFFu

#define TYPE_SHIFT 16u
#define VIS_SHIFT  8u
#define TYPE_MASK  0xFFu

#define MAX_PROBES   128u
#define LOCK_SPINS   5u

layout(std430, binding=0) coherent buffer ChunkHashKeys { uvec2 hash_keys[]; };
layout(std430, binding=1) coherent buffer ChunkHashVals { uint  hash_vals[]; };

// === соответствует C++ VoxelDataGPU (8 байт) ===
struct VoxelData {
    uint type_vis_flags;
    uint color; // RGBA8
};

// === соответствует C++ VoxelWriteGPU (32 байта) ===
// ivec4 (16) + VoxelData (8) + pad0/pad1 (8)
struct VoxelWrite {
    ivec4    world_voxel; // xyz use, w unused
    VoxelData voxel_data;
    uint     _pad0;
    uint     _pad1;
};
layout(std430, binding=2) readonly buffer VoxelWriteList { VoxelWrite writes[]; };

// Воксели в чанках: массив VoxelData (stride = 8)
layout(std430, binding=3) buffer ChunkVoxels { VoxelData voxels[]; };

layout(std430, binding=4) buffer FreeList { uint free_list[]; };
layout(std430, binding=5) buffer FrameCounters { uvec4 counters; }; // x=writeCount, y=dirtyCount, z=cmdCount, w=freeCount

struct ChunkMeta {
    uint used;
    uint key_lo;
    uint key_hi;
    uint dirty_flags;
};
layout(std430, binding=6) buffer ChunkMetaBuf { ChunkMeta meta[]; };

layout(std430, binding=7) buffer EnqueuedBuf { uint enqueued[]; };
layout(std430, binding=8) buffer DirtyListBuf { uint dirty_list[]; };

uniform uint  u_hash_table_size;       // pow2
uniform ivec3 u_chunk_dim;             // (16,16,16)
uniform uint  u_voxels_per_chunk;      // 4096
uniform uint  u_set_dirty_flag_bits;   // e.g. 1u

// ---- pack constants (как у тебя) ----
const uint  BITS   = 21u;
const int   OFFSET = 1 << 20;
const uint  MASK   = (1u << BITS) - 1u;

// ---------- helpers ----------
uint hash_uvec2(uvec2 v) {
    uint x = v.x * 1664525u + 1013904223u;
    uint y = v.y * 22695477u + 1u;
    uint h = x ^ (y + (x << 16) + (x >> 16));
    h ^= h >> 16; h *= 0x7feb352du;
    h ^= h >> 15; h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

// floor-div for divisor > 0
int floor_div(int a, int d) {
    int q = a / d;
    int r = a - q * d;
    if (r != 0 && a < 0) q -= 1;
    return q;
}

int floor_mod(int a, int d) {
    int q = floor_div(a, d);
    return a - q * d; // [0..d-1]
}

uint voxel_index_in_chunk(ivec3 local) {
    return uint((local.z * u_chunk_dim.y + local.y) * u_chunk_dim.x + local.x);
}

uint pop_free_chunk_id() {
    // atomicAdd by 0xFFFFFFFFu is -1 (mod 2^32)
    uint old = atomicAdd(counters.w, 0xFFFFFFFFu);
    if (old == 0u) {
        atomicAdd(counters.w, 1u); // rollback
        return INVALID_ID;
    }
    return free_list[old - 1u];
}

// pack (cx,cy,cz) -> uvec2(lo,hi) без uint64
uvec2 pack_key_uvec2(ivec3 c) {
    uint ux = (uint(c.x + OFFSET) & MASK);
    uint uy = (uint(c.y + OFFSET) & MASK);
    uint uz = (uint(c.z + OFFSET) & MASK);

    uint lo = 0u;
    uint hi = 0u;

    lo |= uz;

    lo |= (uy << BITS);
    if ((BITS + BITS) > 32u) {
        hi |= (uy >> (32u - BITS));
    }

    uint s = 2u * BITS;
    if (s < 32u) {
        lo |= (ux << s);
        if ((s + BITS) > 32u) {
            hi |= (ux >> (32u - s));
        }
    } else if (s == 32u) {
        hi |= ux;
    } else {
        hi |= (ux << (s - 32u));
    }

    return uvec2(lo, hi);
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

uint get_or_create_chunk(uvec2 key) {
    uint mask = u_hash_table_size - 1u;
    uint idx  = hash_uvec2(key) & mask;

    for (uint visited = 0u; visited < MAX_PROBES; /* visited++ делаем только когда реально двигаем idx */) {

        uint v = read_hash_val(idx);

        if (v == SLOT_EMPTY) {
            // пробуем залочить
            if (atomicCompSwap(hash_vals[idx], SLOT_EMPTY, SLOT_LOCKED) == SLOT_EMPTY) {

                uint id = pop_free_chunk_id();
                if (id == INVALID_ID) {
                    atomicExchange(hash_vals[idx], SLOT_EMPTY);
                    return INVALID_ID;
                }

                // publish key -> barrier -> publish id
                hash_keys[idx] = key;
                memoryBarrierBuffer();
                atomicExchange(hash_vals[idx], id);

                meta[id].used = 1u;
                meta[id].key_lo = key.x;
                meta[id].key_hi = key.y;
                meta[id].dirty_flags = u_set_dirty_flag_bits;

                return id;
            }

            // кто-то другой поменял слот — перепроверяем тот же idx
            continue;
        }

        if (v == SLOT_LOCKED) {
            // ждём, но не "съедаем" visited
            uint spins = 0u;
            while (spins++ < 1024u) {
                v = read_hash_val(idx);
                if (v != SLOT_LOCKED) break;
            }
            continue;
        }

        // v = chunkId, key может быть ещё не виден без coherent/barrier
        memoryBarrierBuffer();
        if (all(equal(hash_keys[idx], key))) return v;

        // защитный "повтор" на случай, если key ещё не успел стать видимым
        // (после coherent обычно уже не надо, но как страховка от драйверных нюансов — полезно)
        uint retry = 0u;
        while (retry++ < 8u) {
            memoryBarrierBuffer();
            if (all(equal(hash_keys[idx], key))) return v;

            // если вдруг слот снова залочился/изменился — выходим
            uint v2 = read_hash_val(idx);
            if (v2 != v) break;
        }

        // реально другой ключ → двигаемся дальше и увеличиваем visited
        idx = (idx + 1u) & mask;
        visited++;
    }

    return INVALID_ID;
}



void mark_dirty(uint chunkId) {
    atomicOr(meta[chunkId].dirty_flags, u_set_dirty_flag_bits);

    uint was = atomicExchange(enqueued[chunkId], 1u);
    if (was == 0u) {
        uint di = atomicAdd(counters.y, 1u);
        dirty_list[di] = chunkId;
    }
}

void main() {
    uint wi = gl_GlobalInvocationID.x;
    uint writeCount = counters.x;
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

    uvec2 key = pack_key_uvec2(chunkCoord);
    uint chunkId = get_or_create_chunk(key);
    if (chunkId == INVALID_ID) return;

    uint vi = voxel_index_in_chunk(local);

    // просто пишем VoxelData как есть (8 байт)
    voxels[chunkId * u_voxels_per_chunk + vi] = w.voxel_data;

    mark_dirty(chunkId);
}
