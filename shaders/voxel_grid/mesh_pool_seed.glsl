#version 430
layout(local_size_x = 1) in;

#define INVALID_ID 0xFFFFFFFFu

struct Node {uint page; uint next;};

layout(std430, binding=0) buffer VBHeads { uint vb_heads[]; };
layout(std430, binding=1) buffer VBNodes  { Node vb_nodes[];  };
layout(std430, binding=2) buffer VBState { uint vb_state[]; };
layout(std430, binding=3) buffer VBFreeNodesList  { uint vb_free_nodes_counter; uint vb_free_nodes_list[];  };

layout(std430, binding=4) buffer IBHeads { uint ib_heads[]; };
layout(std430, binding=5) buffer IBNodes  { Node ib_nodes[];  };
layout(std430, binding=6) buffer IBState { uint ib_state[]; };
layout(std430, binding=7) buffer IBFreeNodesList  { uint ib_free_nodes_counter; uint ib_free_nodes_list[];  };

uniform uint u_vb_max_order; // log2(u_vb_pages)
uniform uint u_ib_max_order; // log2(u_ib_pages)
uniform uint u_vb_index_bits;
uniform uint u_ib_index_bits;

// ---- state packing ----
const uint ST_MASK_BITS = 4;
const uint ST_FREE    = 0u;
const uint ST_ALLOC   = 1u;
const uint ST_MERGED  = 2u;
const uint ST_MERGING = 3u;
const uint ST_READY = 4u;
const uint ST_CONCEDED = 5u;
const uint ST_MASK   = (1u << ST_MASK_BITS) - 1u;

// ---- head state packing ----
const uint HEAD_TAG_BITS = 4u; // Чтобы точно не случилось ABA, но если что можно уменьшить
const uint HEAD_TAG_MASK = (1u << HEAD_TAG_BITS) - 1u;
const uint INVALID_HEAD_IDX = INVALID_ID >> HEAD_TAG_BITS;
const uint HEAD_LOCK = 0xFFFFFFFEu;

const uint OP_ALLOC = 0u;
const uint OP_FREE = 1u;

uint pack_state(uint order, uint kind) { return (order << ST_MASK_BITS) | (kind & ST_MASK); }
uint pack_head(uint start, uint tag) {return (start << HEAD_TAG_BITS) | (tag & HEAD_TAG_MASK); };

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

    uint vb_node_id = vb_free_nodes_counter - 1u;
    vb_free_nodes_counter -= 1u;

    vb_nodes[vb_node_id].page = 0u;
    vb_nodes[vb_node_id].next = INVALID_ID;
    vb_heads[u_vb_max_order] = pack_head(vb_node_id, 0u);
    vb_state[0u] = pack_state(u_vb_max_order, ST_FREE);

    uint ib_node_id = ib_free_nodes_counter - 1u;
    ib_free_nodes_counter -= 1u;

    ib_nodes[ib_node_id].page = 0u;
    ib_nodes[ib_node_id].next = INVALID_ID;
    ib_heads[u_ib_max_order] = pack_head(ib_node_id, 0u);
    ib_state[0u] = pack_state(u_ib_max_order, ST_FREE);

    



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
