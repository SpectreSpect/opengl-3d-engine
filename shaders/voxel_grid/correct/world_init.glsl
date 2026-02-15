#version 430
layout(local_size_x = 256) in;

#define SLOT_EMPTY 0xFFFFFFFFu
#define SLOT_TOMB  0xFFFFFFFDu

layout(std430, binding=0) coherent buffer ChunkHashKeys { uvec2 hash_keys[]; };
layout(std430, binding=1) coherent buffer ChunkHashVals { uint  hash_vals[]; };

layout(std430, binding=4) buffer FreeList { uint free_list[]; };
layout(std430, binding=5) buffer FrameCounters { uvec4 counters; };

struct ChunkMeta { uint used; uint key_lo; uint key_hi; uint dirty_flags; };
layout(std430, binding=6) buffer ChunkMetaBuf { ChunkMeta meta[]; };

layout(std430, binding=7) buffer EnqueuedBuf { uint enqueued[]; };

uniform uint u_hash_table_size;
uniform uint u_max_chunks;

void main() {
    uint i = gl_GlobalInvocationID.x;

    if (i < u_hash_table_size) {
        hash_vals[i] = SLOT_EMPTY;
        hash_keys[i] = uvec2(0u, 0u);
    }

    if (i < u_max_chunks) {
        free_list[i]   = i;
        enqueued[i]    = 0u;

        meta[i].used       = 0u;
        meta[i].key_lo     = 0u;
        meta[i].key_hi     = 0u;
        meta[i].dirty_flags= 0u;
    }

    if (i == 0u) {
        counters = uvec4(0u, 0u, 0u, u_max_chunks); // writeCount=0 dirtyCount=0 cmdCount=0 freeCount=max
    }
}
