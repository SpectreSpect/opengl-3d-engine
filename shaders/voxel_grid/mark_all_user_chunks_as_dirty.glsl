#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) buffer ChunkMetaBuf { ChunkMeta meta[]; };
layout(std430, binding=1) buffer EnqueuedBuf { uint enqueued[]; };
layout(std430, binding=2) buffer DirtyListBuf { uint dirty_count; uint dirty_list[]; };
layout(std430, binding=3) buffer FrameCountersBuf { FrameCounters counters; };

uniform uint u_max_chunks;
uniform uint u_set_dirty_flag_bits; 

void mark_dirty(uint chunkId) {
    uint was = atomicCompSwap(enqueued[chunkId], 0u, 1u);
    if (was == 0u) {
        atomicOr(meta[chunkId].dirty_flags, u_set_dirty_flag_bits);
        uint di = atomicAdd(dirty_count, 1u);
        dirty_list[di] = chunkId;
    }
}

void main() {
    uint chunk_id = gl_GlobalInvocationID.x;
    if (chunk_id >= u_max_chunks) return;

    if (meta[chunk_id].used == 1u)
        mark_dirty(chunk_id);
}
