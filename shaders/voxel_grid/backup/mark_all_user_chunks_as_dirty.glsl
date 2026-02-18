#version 430
layout(local_size_x = 256) in;

struct ChunkMeta { uint used; uint key_lo; uint key_hi; uint dirty_flags; };
layout(std430, binding=6) buffer ChunkMetaBuf { ChunkMeta meta[]; };

layout(std430, binding=7) buffer EnqueuedBuf { uint enqueued[]; };
layout(std430, binding=8) buffer DirtyListBuf { uint dirty_list[]; };
layout(std430, binding=5) buffer FrameCounters { uvec4 counters; }; // y=dirtyCount

uniform uint u_max_chunks;
uniform uint u_set_dirty_flag_bits; 

void mark_dirty(uint chunkId) {
    atomicOr(meta[chunkId].dirty_flags, u_set_dirty_flag_bits);
    uint was = atomicExchange(enqueued[chunkId], 1u);
    if (was == 0u) {
        uint di = atomicAdd(counters.y, 1u);
        dirty_list[di] = chunkId;
    }
}

void main() {
    uint chunk_id = gl_GlobalInvocationID.x;
    if (chunk_id >= u_max_chunks) return;

    if (meta[chunk_id].used == 1u)
        mark_dirty(chunk_id);
}
