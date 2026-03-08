#version 430
layout(local_size_x = 256) in;

struct FrameCounters {
    uint write_count; 
    uint dirty_count;
    uint cmd_count;
    uint free_count;
    uint failed_dirty_count;
    uint count_vb_free_pages;
    uint count_ib_free_pages;
};
layout(std430, binding=0) buffer FrameCountersBuf { FrameCounters counters; }; // y = dirtyCount
layout(std430, binding=1) readonly buffer DirtyListBuf { uint dirty_list[]; };

layout(std430, binding=2) buffer DirtyQuadCountBuf { uint dirty_quad_count[]; }; // indexed by dirtyIdx
layout(std430, binding=3) buffer EmitCounterBuf { uint emit_counter[]; };        // indexed by chunkId

uniform uvec3 u3_chunk_size;

uint div_up_u32(uint a, uint b) { return (a + b - 1u) / b; }

uint max(uint a, uint b) {
    return a > b ? a : b;
}

void main() {
    // if (gl_GlobalInvocationID.x == 0) {
    //     uint vox_per_chunk = u3_chunk_size.x * u3_chunk_size.y * u3_chunk_size.z;
    //     uint vox_groups = div_up_u32(vox_per_chunk, 256u);
    //     dispatch_buf = uvec3(vox_groups, max(counters.dirty_count, 1u), 1u);
    // }

    uint dirtyIdx = gl_GlobalInvocationID.x;
    uint dirtyCount = counters.dirty_count;
    if (dirtyIdx >= dirtyCount) return;

    dirty_quad_count[dirtyIdx] = 0u;

    uint chunkId = dirty_list[dirtyIdx];
    emit_counter[chunkId] = 0u;
}
