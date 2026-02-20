#version 430
layout(local_size_x = 256) in;

struct FrameCounters {uint write_count; uint dirty_count; uint cmd_count; uint free_count; uint failed_dirty_count; };
layout(std430, binding=5) buffer FrameCountersBuf { FrameCounters counters; }; // y = dirtyCount
layout(std430, binding=8) readonly buffer DirtyListBuf { uint dirty_list[]; };

layout(std430, binding=11) buffer DirtyQuadCountBuf { uint dirty_quad_count[]; }; // indexed by dirtyIdx
layout(std430, binding=12) buffer EmitCounterBuf { uint emit_counter[]; };        // indexed by chunkId

void main() {
    uint dirtyIdx = gl_GlobalInvocationID.x;
    uint dirtyCount = counters.dirty_count;
    if (dirtyIdx >= dirtyCount) return;

    dirty_quad_count[dirtyIdx] = 0u;

    uint chunkId = dirty_list[dirtyIdx];
    emit_counter[chunkId] = 0u;
}
