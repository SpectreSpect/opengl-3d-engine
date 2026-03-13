#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) buffer MeshBuffersStatusBuf { uint is_vb_full; uint is_ib_full; };
layout(std430, binding=1) readonly buffer DirtyListBuf { uint dirty_count; uint dirty_list[]; };
layout(std430, binding=2) buffer DirtyQuadCountBuf { uint dirty_quad_count[]; };
layout(std430, binding=3) buffer EmitCounterBuf { uint emit_counter[]; };

// ----- include -----
#include "common/common.glsl"
// -------------------

void main() {
    uint dirtyIdx = gl_GlobalInvocationID.x;
    uint dirtyCount = dirty_count;
    if (dirtyIdx >= dirtyCount) return;

    dirty_quad_count[dirtyIdx] = 0u;

    uint chunkId = dirty_list[dirtyIdx];
    emit_counter[chunkId] = 0u;
}
