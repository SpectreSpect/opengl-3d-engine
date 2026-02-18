#version 430
layout(local_size_x = 1) in;

layout(std430, binding=18) buffer VBHeads { uint vb_heads[]; };
layout(std430, binding=19) buffer VBNext  { uint vb_next[];  };
layout(std430, binding=20) buffer VBState { uint vb_state[]; };

layout(std430, binding=21) buffer IBHeads { uint ib_heads[]; };
layout(std430, binding=22) buffer IBNext  { uint ib_next[];  };
layout(std430, binding=23) buffer IBState { uint ib_state[]; };

uniform uint u_vb_max_order; // log2(u_vb_pages)
uniform uint u_ib_max_order; // log2(u_ib_pages)
uniform uint u_vb_index_bits;
uniform uint u_ib_index_bits;

const uint ST_MASK_BITS = 4u;
const uint ST_FREE     = 0u;
const uint ST_ALLOC    = 1u;
const uint ST_MERGED   = 2u;
const uint ST_MERGING  = 3u;
const uint ST_READY    = 4u;
const uint ST_CONCEDED = 5u;
const uint ST_MASK   = (1 << ST_MASK_BITS) - 1;

#define INVALID_ID 0xFFFFFFFFu

uint pack_state(uint order, uint kind) { return (order << ST_MASK_BITS) | (kind & ST_MASK); }

uint vb_mask() { return (1u << u_vb_index_bits) - 1u; } // INVALID_ID
uint vb_pack_head(uint idx, uint tag) { return (tag << u_vb_index_bits) | idx; }
uint vb_head_idx(uint h) { return h & vb_mask(); }
uint vb_head_tag(uint h) { return h >> u_vb_index_bits; }

uint ib_mask() { return (1u << u_ib_index_bits) - 1u; } // INVALID_ID
uint ib_pack_head(uint idx, uint tag) { return (tag << u_ib_index_bits) | idx; }
uint ib_head_idx(uint h) { return h & ib_mask(); }
uint ib_head_tag(uint h) { return h >> u_ib_index_bits; }

// void vb_push_free(uint order, uint startPage) {
//     for (uint it = 0u; it < 64u; ++it) {
//         uint oldH = atomicAdd(vb_heads[order], 0u);
//         uint oldIdx = vb_head_idx(oldH);
//         uint oldTag = vb_head_tag(oldH);

//         // next можно писать обычным store, но тогда barrier нужен
//         vb_next[startPage] = oldIdx;
//         memoryBarrierBuffer(); // publish vb_next before publishing head

//         uint newH = vb_pack_head(startPage, oldTag + 1u);
//         if (atomicCompSwap(vb_heads[order], oldH, newH) == oldH) {
//             atomicExchange(vb_state[startPage], pack_state(order, ST_FREE));
//             return;
//         }
//     }
    
//     atomicExchange(vb_state[startPage], pack_state(order, ST_FREE));
// }

// void ib_push_free(uint order, uint startPage) {
//     uint invalid = ib_mask();
//     for (uint it = 0u; it < 64u; ++it) {
//         uint oldH = atomicAdd(ib_heads[order], 0u);
//         uint oldIdx = ib_head_idx(oldH);
//         uint oldTag = ib_head_tag(oldH);

//         ib_next[startPage] = oldIdx;
//         memoryBarrierBuffer();

//         uint newH = ib_pack_head(startPage, oldTag + 1u);
//         if (atomicCompSwap(ib_heads[order], oldH, newH) == oldH) {
//             atomicExchange(ib_state[startPage], pack_state(order, ST_FREE));
//             return;
//         }
//     }

//     atomicExchange(ib_state[startPage], pack_state(order, ST_FREE));
// }

void main() {
    // uint vb_seed_order = (u_vb_max_order > 8u) ? (u_vb_max_order - 8u) : 0u;
    // uint vb_blockPages = 1u << vb_seed_order;
    // uint vb_blocks = 1u << (u_vb_max_order - vb_seed_order);

    vb_heads[u_vb_max_order] = 0u;
    vb_state[0u] = pack_state(u_vb_max_order, ST_FREE);
    vb_next[0u] = INVALID_ID;

    ib_heads[u_ib_max_order] = 0u;
    ib_state[0u] = pack_state(u_ib_max_order, ST_FREE);
    ib_next[0u] = INVALID_ID;
    



    // for (uint b = 0u; b < vb_blocks; ++b) {
    //     uint start = b * vb_blockPages;
    //     // сделать этот startPage свободным блоком seed_order
    //     vb_next[start] = vb_mask();
    //     vb_state[start] = pack_state(vb_seed_order, ST_FREE);
    //     vb_push_free(vb_seed_order, start);
    // }

    // uint ib_seed_order = (u_ib_max_order > 8u) ? (u_ib_max_order - 8u) : 0u;
    // uint ib_blockPages = 1u << ib_seed_order;
    // uint ib_blocks = 1u << (u_ib_max_order - ib_seed_order);

    // for (uint b = 0u; b < ib_blocks; ++b) {
    //     uint start = b * ib_blockPages;
    //     // сделать этот startPage свободным блоком seed_order
    //     ib_next[start] = ib_mask();
    //     ib_state[start] = pack_state(ib_seed_order, ST_FREE);
    //     ib_push_free(ib_seed_order, start);
    // }
}
