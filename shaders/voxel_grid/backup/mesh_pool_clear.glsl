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
uniform uint u_vb_index_bits;
uniform uint u_ib_index_bits;

uint vb_mask() { return (1u << u_vb_index_bits) - 1u; } // INVALID_ID
uint ib_mask() { return (1u << u_ib_index_bits) - 1u; } // INVALID_ID

uint vb_pack_head(uint idx, uint tag) { return (tag << u_vb_index_bits) | idx; }
uint ib_pack_head(uint idx, uint tag) { return (tag << u_ib_index_bits) | idx; }

const uint ST_MASK_BITS = 4;
const uint ST_FREE    = 0u;
const uint ST_ALLOC   = 1u;
const uint ST_LOCKED  = 2u;
const uint ST_PUSHING = 3u;
const uint ST_MASK   = (1 << ST_MASK_BITS) - 1;

uint pack_state(uint order, uint kind) { return (order << ST_MASK_BITS) | (kind & ST_MASK); }

void main() {
    uint i = gl_GlobalInvocationID.x;

    if (i < u_vb_pages) {
        vb_next[i]  = vb_mask();
        vb_state[i] = pack_state(0, ST_LOCKED); // "ничего"
    }
    if (i < u_ib_pages) {
        ib_next[i]  = ib_mask();
        ib_state[i] = pack_state(0, ST_LOCKED);
    }
    if (i < u_vb_heads_count) vb_heads[i] = vb_pack_head(vb_mask(), 0u);
    if (i < u_ib_heads_count) ib_heads[i] = ib_pack_head(ib_mask(), 0u);

    if (i < u_max_chunks) {
        chunk_alloc[i] = uvec4(INVALID_ID, 0u, INVALID_ID, 0u);
    }
}
