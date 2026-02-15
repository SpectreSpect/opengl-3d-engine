#version 430
layout(local_size_x = 256) in;

#define INVALID_ID 0xFFFFFFFFu

layout(std430, binding=18) buffer VBHeads { uint vb_heads[]; };
layout(std430, binding=19) buffer VBNext  { uint vb_next[];  };
layout(std430, binding=20) buffer VBState { uint vb_state[]; };

layout(std430, binding=21) buffer IBHeads { uint ib_heads[]; };
layout(std430, binding=22) buffer IBNext  { uint ib_next[];  };
layout(std430, binding=23) buffer IBState { uint ib_state[]; };

layout(std430, binding=24) buffer ChunkMeshAllocBuf { uvec4 chunk_alloc[]; };

uniform uint u_vb_pages;
uniform uint u_ib_pages;
uniform uint u_vb_heads_count; // = VB_MAX_ORDER+1
uniform uint u_ib_heads_count; // = IB_MAX_ORDER+1
uniform uint u_max_chunks;

void main() {
    uint i = gl_GlobalInvocationID.x;

    if (i < u_vb_pages) {
        vb_next[i]  = INVALID_ID;
        vb_state[i] = INVALID_ID; // "ничего"
    }
    if (i < u_ib_pages) {
        ib_next[i]  = INVALID_ID;
        ib_state[i] = INVALID_ID;
    }
    if (i < u_vb_heads_count) vb_heads[i] = INVALID_ID;
    if (i < u_ib_heads_count) ib_heads[i] = INVALID_ID;

    if (i < u_max_chunks) {
        chunk_alloc[i] = uvec4(INVALID_ID, 0u, INVALID_ID, 0u);
    }
}
