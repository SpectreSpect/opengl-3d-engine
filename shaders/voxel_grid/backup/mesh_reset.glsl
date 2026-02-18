#version 430
layout(local_size_x = 256) in;

layout(std430, binding=5) buffer FrameCounters { uvec4 counters; }; // y = dirtyCount
layout(std430, binding=8) readonly buffer DirtyListBuf { uint dirty_list[]; };

layout(std430, binding=11) buffer DirtyQuadCountBuf { uint dirty_quad_count[]; }; // indexed by dirtyIdx
layout(std430, binding=12) buffer EmitCounterBuf { uint emit_counter[]; };        // indexed by chunkId

void main() {
    uint dirtyIdx = gl_GlobalInvocationID.x;
    uint dirtyCount = counters.y;
    if (dirtyIdx >= dirtyCount) return;

    dirty_quad_count[dirtyIdx] = 0u;

    uint chunkId = dirty_list[dirtyIdx];
    emit_counter[chunkId] = 0u;
}
