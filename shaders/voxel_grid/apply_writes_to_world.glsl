#version 430
layout(local_size_x = 256) in;

#define SLOT_EMPTY   0xFFFFFFFFu
#define SLOT_LOCKED  0xFFFFFFFEu
#define INVALID_ID   0xFFFFFFFFu
#define SLOT_TOMB    0xFFFFFFFDu

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

struct FrameCounters {
    uint write_count; 
    uint dirty_count;
    uint cmd_count;
    uint free_count;
    uint failed_dirty_count;
    uint count_vb_free_pages;
    uint count_ib_free_pages;
};
layout(std430, binding=5) buffer FrameCountersBuf { FrameCounters counters; };

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
uint firstTomb = 0xFFFFFFFFu;

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
    uint old = atomicAdd(counters.free_count, 0xFFFFFFFFu);
    if (old == 0u) {
        atomicAdd(counters.free_count, 1u); // rollback
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

// table_is_changing = 0u позволяет ускорить работу функции. Но эту оптимизацию (как следует из названия переменной)
// можно использовать только в том случае, если таблица одновременно не изменяется (а только читается)! Иначе всё сломается.
uint lookup_chunk(uvec2 key, uint table_is_changing = 0u) {
    uint mask = u_hash_table_size - 1u;
    uint idx  = hash_uvec2(key) & mask;

    for (uint probe = 0u; probe < MAX_PROBES;) {
        uint v = atomicAdd(hash_vals[idx], 0u);

        if (v == SLOT_LOCKED) continue;

        if (v == SLOT_TOMB) {
            idx = (idx + 1u) & mask;
            probe++;
            continue;
        }

        if (v == SLOT_EMPTY) return INVALID_ID;

        // Если мы сюда дошли, значит в слоте стоит чья-то запись. Нужно прочитать ключ
        // Но чтобы прочитать ключ необходимо тоже залочить! (иначе во время прочтения его состояние может уже измениться) 
        memoryBarrierBuffer();

        if (all(equal(hash_keys[idx], key))) 
            return v;

        // if (atomicCompSwap(hash_vals[idx], v, SLOT_LOCKED) == v) {
        //     // Залочили слот - можем читать ключ
        //     if (all(equal(hash_keys[idx], key))) {
        //         atomicExchange(hash_vals[idx], v); // Убираем блокировку
        //         return v;
        //     }

        //     atomicExchange(hash_vals[idx], v); // Убираем блокировку
        // } else {
        //     continue; // Не получилось захватить. Попробуем ещё раз.
        // }
        
        idx = (idx + 1u) & mask;
        probe++;
    }

    return INVALID_ID;
}

// ---------------- get_or_create ----------------
bool get_or_create_chunk(uvec2 key, out uint outId, out bool created) {
    uint mask = u_hash_table_size - 1u;
    uint idx  = hash_uvec2(key) & mask;
    uint last_tomb_id = INVALID_ID;

    tomb_list_head_id = INVALID_ID;
    tomb_list_tail_id = INVALID_ID;
    tomb_list_count_elements = 0u;

    for (uint probe = 0u; probe < MAX_PROBES + 1u;) {
        uint v = atomicAdd(hash_vals[idx], 0u);

        if (v == SLOT_EMPTY || probe == MAX_PROBES) {
            if (probe >= MAX_PROBES) idx = INVALID_ID;

            // Не нашли элемент. Значит создаём (в приоритете в first_tomb)
            if (last_tomb_id == INVALID_ID) last_tomb_id = pop_tail_tomb_id();

            uint idx_to_create = last_tomb_id == INVALID_ID ? idx : last_tomb_id;
            uint slot_state = last_tomb_id == INVALID_ID ? SLOT_EMPTY : SLOT_TOMB;

            if (idx_to_create == INVALID_ID) return false; // Нет ни SLOT_EMPTY, ни SLOT_TOMB в очереди

            uint prev = atomicCompSwap(hash_vals[idx_to_create], slot_state, SLOT_LOCKED);
            if (prev != slot_state) {
                // кто-то успел — перепроверяем этот же idx

                if (last_tomb_id != INVALID_ID && prev != SLOT_LOCKED) {
                    // Условие говорит, что мы проверяем tomb_check_list_counter - 1u слот и он оказался занят
                    last_tomb_id = INVALID_ID;
                    continue;
                }
                else {
                    continue;
                }
            }

            uint id = pop_free_chunk_id();
            if (id == INVALID_ID) {
                atomicExchange(hash_vals[idx_to_create], slot_state);
                return false;
            }

            // meta подготовим ДО публикации id
            meta[id].used       = 1u;
            meta[id].key_lo     = key.x;
            meta[id].key_hi     = key.y;
            meta[id].dirty_flags= 0u;
            enqueued[id]        = 0u;

            // публикуем key
            hash_keys[idx_to_create] = key;

            // гарантируем, что key/meta видимы до публикации id
            memoryBarrierBuffer();

            // публикуем id (и одновременно "анлочим")
            atomicExchange(hash_vals[idx_to_create], id);

            outId = id;
            created = true;
            return true;
        }

        if (v == SLOT_LOCKED) continue;

        if (v == SLOT_TOMB) {
            push_tomb_id(idx);
            idx = (idx + 1u) & mask;
            probe++;
            continue;
        }

        // Если мы сюда дошли, значит в слоте стоит чья-то запись. Нужно прочитать ключ
        // Но чтобы прочитать ключ необходимо тоже залочить! (иначе во время прочтения его состояние может уже измениться) 
        memoryBarrierBuffer();

        if (all(equal(hash_keys[idx], key))) {
            outId = v;
            created = false;
            return true;
        }
            

        // if (atomicCompSwap(hash_vals[idx], v, SLOT_LOCKED) == v) {
        //     // Залочили слот - можем читать ключ
        //     if (all(equal(hash_keys[idx], key))) {
        //         outId = v;
        //         created = false;
        //         atomicExchange(hash_vals[idx], v); // Убираем блокировку
        //         return true;
        //     }

        //     atomicExchange(hash_vals[idx], v); // Убираем блокировку
        // } else {
        //     continue; // Не получилось захватить. Попробуем ещё раз.
        // }
        
        idx = (idx + 1u) & mask;
        probe++;
    }

    return false;
}



void mark_dirty(uint chunkId) {
    atomicOr(meta[chunkId].dirty_flags, u_set_dirty_flag_bits);

    uint was = atomicExchange(enqueued[chunkId], 1u);
    if (was == 0u) {
        uint di = atomicAdd(counters.dirty_count, 1u);
        dirty_list[di] = chunkId;
    }
}

void main() {
    uint wi = gl_GlobalInvocationID.x;
    uint writeCount = counters.write_count;
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
