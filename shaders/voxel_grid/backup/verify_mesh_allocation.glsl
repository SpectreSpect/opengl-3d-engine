#version 430
layout(local_size_x = 256) in;

#define INVALID_ID 0xFFFFFFFFu


struct ChunkMeshAlloc {uint v_startPage; uint v_order; uint needV; uint i_startPage; uint i_order; uint needI; uint need_rebuild; };
layout(std430, binding=0) readonly buffer LocalChunkMeshAllocBuf { ChunkMeshAlloc chunk_alloc_local[]; }; 
layout(std430, binding=1) buffer GlobalChunkMeshAllocBuf { ChunkMeshAlloc chunk_alloc_global[]; }; 
layout(std430, binding=3) readonly buffer DirtyListBuf { uint dirty_list[]; };

struct FrameCounters {
    uint write_count; 
    uint dirty_count;
    uint cmd_count;
    uint free_count;
    uint failed_dirty_count;
    uint count_vb_free_pages;
    uint count_ib_free_pages;
};
layout(std430, binding=4) buffer FrameCountersBuf { FrameCounters counters; };

uniform uint u_min_free_pages;

void main() {
    uint dirtyIdx = gl_GlobalInvocationID.x;
    uint dirtyCount = counters.dirty_count;
    if (dirtyIdx >= dirtyCount) return;

    if (counters.count_vb_free_pages >= u_min_free_pages && counters.count_ib_free_pages >= u_min_free_pages) {
        uint chunkId = dirty_list[dirtyIdx];
        chunk_alloc_global[chunkId] = chunk_alloc_local[dirtyIdx];
    }
}
