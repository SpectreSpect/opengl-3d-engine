#version 430

#define INVALID_ID 0xFFFFFFFFu

layout(local_size_x = 256) in;

struct FrameCounters {uint write_count; uint dirty_count; uint cmd_count; uint free_count; uint failed_dirty_count; };
layout(std430, binding=0) buffer FrameCountersBuf { FrameCounters counters; }; // y = dirtyCount
layout(std430, binding=1) readonly buffer DirtyListBuf { uint dirty_list[]; };

layout(std430, binding=2) buffer EnqueuedBuf { uint enqueued[]; };

struct ChunkMeta { uint used; uint key_lo; uint key_hi; uint dirty_flags; };
layout(std430, binding=3) buffer ChunkMetaBuf { ChunkMeta meta[]; };

struct ChunkMeshAlloc {uint v_startPage; uint v_order; uint needV; uint i_startPage; uint i_order; uint needI; uint need_rebuild; };
layout(std430, binding=4) buffer ChunkMeshAllocBuf { ChunkMeshAlloc chunk_alloc[]; }; 

layout(std430, binding=5) buffer FailedDirtyListBuf { uint failed_dirty_list[]; }; 

uniform uint u_dirty_flag_bits; // например 1u

void main() {
    uint dirtyIdx = gl_GlobalInvocationID.x;
    uint dirtyCount = counters.dirty_count;
    if (dirtyIdx >= dirtyCount) return;

    uint chunkId = dirty_list[dirtyIdx];
    
    if (chunk_alloc[chunkId].v_startPage == INVALID_ID || chunk_alloc[chunkId].i_startPage == INVALID_ID) {
        if (enqueued[chunkId] != 2u) {
            uint idx = atomicAdd(counters.failed_dirty_count, 1u);
            failed_dirty_list[idx] = chunkId;
            enqueued[chunkId] = 2u;
        }
    } else {
        // разрешить повторно enqueue
        enqueued[chunkId] = 0u;
    }

    // снять dirty флаг(и)
    meta[chunkId].dirty_flags &= ~u_dirty_flag_bits;
}
