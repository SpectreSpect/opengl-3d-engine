#version 430
layout(local_size_x = 1) in;

#define SLOT_EMPTY   0xFFFFFFFFu
#define SLOT_LOCKED  0xFFFFFFFEu
#define SLOT_TOMB    0xFFFFFFFDu
#define INVALID_ID   0xFFFFFFFFu

#define MAX_PROBES   128u
#define LOCK_SPINS   64u

layout(std430, binding=0) coherent buffer ChunkHashKeys { uvec2 hash_keys[]; };
layout(std430, binding=1) coherent buffer ChunkHashVals { uint  hash_vals[]; };

layout(std430, binding=4) buffer FreeList { uint free_list[]; };
layout(std430, binding=5) coherent buffer FrameCounters { uvec4 counters; }; // w=freeCount

struct ChunkMeta { uint used; uint key_lo; uint key_hi; uint dirty_flags; };
layout(std430, binding=6) coherent buffer ChunkMetaBuf { ChunkMeta meta[]; };

layout(std430, binding=7) coherent buffer EnqueuedBuf { uint enqueued[]; };

struct ChunkMeshMeta { uint first_index; uint index_count; uint base_vertex; uint mesh_valid; };
layout(std430, binding=9) coherent buffer ChunkMeshMetaBuf { ChunkMeshMeta mesh_meta[]; };

layout(std430, binding=16) coherent buffer BucketHeads { uint bucket_heads[]; };
layout(std430, binding=17) coherent buffer BucketNext  { uint bucket_next[]; };

uniform uint u_hash_table_size;
uniform uint u_bucket_count;
uniform uint u_min_free;    // сколько свободных хотим иметь
uniform uint u_max_evict;   // safety limit за один вызов

uint hash_uvec2(uvec2 v) {
    uint x = v.x * 1664525u + 1013904223u;
    uint y = v.y * 22695477u + 1u;
    uint h = x ^ (y + (x << 16) + (x >> 16));
    h ^= h >> 16; h *= 0x7feb352du;
    h ^= h >> 15; h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

bool remove_from_table(uvec2 key, uint chunkId) {
    uint mask = u_hash_table_size - 1u;
    uint idx  = hash_uvec2(key) & mask;

    for (uint probe = 0u; probe < MAX_PROBES; ) {
        uint v = atomicAdd(hash_vals[idx], 0u);

        if (v == SLOT_LOCKED) {
            uint spins = 0u;
            while (spins++ < 1024u) {
                v = atomicAdd(hash_vals[idx], 0u);
                if (v != SLOT_LOCKED) break;
            }
            if (v == SLOT_LOCKED) return false; 
        }

        if (v == SLOT_EMPTY) return false;

        if (v == SLOT_TOMB) { idx = (idx + 1u) & mask; probe++; continue; }

        if (v == chunkId && all(equal(hash_keys[idx], key))) {
            uint prev = atomicCompSwap(hash_vals[idx], v, SLOT_TOMB);
            if (prev == v) return true;
            continue;
        }

        idx = (idx + 1u) & mask;
        probe++;
    }
    return false;
}

uint pop_bucket(uint b) {
    for (;;) {
        uint h = atomicAdd(bucket_heads[b], 0u);
        if (h == INVALID_ID) return INVALID_ID;

        memoryBarrierBuffer();
        uint nxt = bucket_next[h];

        uint prev = atomicCompSwap(bucket_heads[b], h, nxt);
        if (prev == h) return h;
    }
}

void main() {
    if (gl_GlobalInvocationID.x != 0u) return;

    uint freeCount = atomicAdd(counters.w, 0u);
    if (freeCount >= u_min_free) return;

    uint evicted = 0u;

    while (freeCount < u_min_free && evicted < u_max_evict) {
        uint victim = INVALID_ID;

        // худшие бакеты — с конца (больший bucket == дальше)
        for (int bi = int(u_bucket_count) - 1; bi >= 0; --bi) {
            victim = pop_bucket(uint(bi));
            if (victim != INVALID_ID) break;
        }

        if (victim == INVALID_ID) break; // не нашли

        // мог уже стать свободным/неактивным
        if (meta[victim].used == 0u) continue;

        uvec2 key = uvec2(meta[victim].key_lo, meta[victim].key_hi);

        // выкидываем из таблицы
        remove_from_table(key, victim);

        // освобождаем метаданные
        meta[victim].used = 0u;
        meta[victim].dirty_flags = 0u;
        enqueued[victim] = 0u;

        mesh_meta[victim].mesh_valid = 0u;
        mesh_meta[victim].index_count = 0u;

        // пушим обратно в free_list (stack)
        uint idx = atomicAdd(counters.w, 1u);
        free_list[idx] = victim;

        freeCount++;
        evicted++;
    }
}
