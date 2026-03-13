#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) coherent buffer ChunkHashKeys { uvec2 hash_keys[]; };
layout(std430, binding=1) coherent buffer ChunkHashVals { uint  hash_vals[]; };
layout(std430, binding=2) buffer FreeList { uint free_count; uint free_list[]; };
layout(std430, binding=3) buffer MeshBuffersStatusBuf { uint is_vb_full; uint is_ib_full; };
layout(std430, binding=4) buffer ChunkMetaBuf { ChunkMeta meta[]; };
layout(std430, binding=5) buffer EnqueuedBuf { uint enqueued[]; };
layout(std430, binding=6) coherent buffer BucketHeads { uint bucket_heads[]; };
layout(std430, binding=7) coherent buffer BucketNext  { uint bucket_next[]; };
layout(std430, binding=8) buffer ChunkMeshAllocBuf { ChunkMeshAlloc chunk_alloc[]; };
layout(std430, binding=9) buffer EvictedChunksList { uint evicted_chunks_counter; uint evicted_chunks_list[]; };

uniform uint u_hash_table_size;
uniform uint u_bucket_count;

// ----- include -----
#include "common/common.glsl"

#define NOT_INCLUDE_GET_OR_CREATE
#include "common/hash_table.glsl"
// -------------------

// ABA проблемы не будет, так как везде используется либо только pop, либо только push (поэтому теги на heads пока не нужны)
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
    uint enviction_id = gl_GlobalInvocationID.x;
    if (enviction_id >= evicted_chunks_counter) return;

    uint victim = INVALID_ID;
    for (;;) {
        // худшие бакеты — с конца (больший bucket == дальше)
        for (int bi = int(u_bucket_count) - 1; bi >= 0; --bi) {
            victim = pop_bucket(uint(bi));
            if (victim != INVALID_ID) break;
        }

        if (victim == INVALID_ID)  {
            // не нашли
            evicted_chunks_list[enviction_id] = INVALID_ID;
            return;
        }

        // мог уже стать свободным/неактивным
        if (meta[victim].used == 0u) continue;

        uvec2 key = uvec2(meta[victim].key_lo, meta[victim].key_hi);

        // выкидываем из таблицы
        remove_from_table(key);

        // освобождаем метаданные
        meta[victim].used = 0u;
        meta[victim].dirty_flags = 0u;
        enqueued[victim] = 0u;

        // пушим обратно в free_list (stack)
        uint idx = atomicAdd(free_count, 1u);
        free_list[idx] = victim;
        
        evicted_chunks_list[enviction_id] = victim;
        return;
    }
}
