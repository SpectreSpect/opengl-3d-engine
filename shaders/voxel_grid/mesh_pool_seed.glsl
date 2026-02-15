#version 430
layout(local_size_x = 1) in;

#define INVALID_ID 0xFFFFFFFFu

const uint ST_FREE   = 0u;
uint pack_state(uint order, uint kind) { return (order << 2u) | (kind & 3u); }

layout(std430, binding=18) buffer VBHeads { uint vb_heads[]; };
layout(std430, binding=19) buffer VBNext  { uint vb_next[];  };
layout(std430, binding=20) buffer VBState { uint vb_state[]; };

layout(std430, binding=21) buffer IBHeads { uint ib_heads[]; };
layout(std430, binding=22) buffer IBNext  { uint ib_next[];  };
layout(std430, binding=23) buffer IBState { uint ib_state[]; };

uniform uint u_vb_max_order; // log2(u_vb_pages)
uniform uint u_ib_max_order; // log2(u_ib_pages)

void main() {
    // VB: один большой свободный блок [0 .. 2^K-1]
    vb_heads[u_vb_max_order] = 0u;
    vb_next[0u]  = INVALID_ID;
    vb_state[0u] = pack_state(u_vb_max_order, ST_FREE);

    // IB: то же самое
    ib_heads[u_ib_max_order] = 0u;
    ib_next[0u]  = INVALID_ID;
    ib_state[0u] = pack_state(u_ib_max_order, ST_FREE);
}
