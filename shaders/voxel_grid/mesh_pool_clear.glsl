#version 430
layout(local_size_x = 256) in;

#define INVALID_ID 0xFFFFFFFFu

layout(std430, binding=0) buffer VBHeads { uint vb_heads[]; };
layout(std430, binding=1) buffer VBState { uint vb_state[]; };
layout(std430, binding=2) buffer VBFreeNodesList  { uint vb_free_nodes_counter; uint vb_free_nodes_list[];  };

layout(std430, binding=3) buffer IBHeads { uint ib_heads[]; };
layout(std430, binding=4) buffer IBState { uint ib_state[]; };
layout(std430, binding=5) buffer IBFreeNodesList  { uint ib_free_nodes_counter; uint ib_free_nodes_list[];  };

struct ChunkMeshAlloc {uint v_startPage; uint v_order; uint needV; uint i_startPage; uint i_order; uint needI; uint need_rebuild; };
layout(std430, binding=6) buffer ChunkMeshAllocBuf { ChunkMeshAlloc chunk_alloc[]; };

uniform uint u_vb_pages;
uniform uint u_ib_pages;
uniform uint u_vb_nodes;
uniform uint u_ib_nodes;
uniform uint u_vb_heads_count; // = VB_MAX_ORDER+1
uniform uint u_ib_heads_count; // = IB_MAX_ORDER+1
uniform uint u_max_chunks;
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

uint pack_state(uint order, uint kind) { return (order << ST_MASK_BITS) | (kind & ST_MASK); };
uint pack_head(uint start, uint tag) {return (start << HEAD_TAG_BITS) | (tag & HEAD_TAG_MASK); };

void main() {
    uint i = gl_GlobalInvocationID.x;

    if (i == 0u) {
        vb_free_nodes_counter = u_vb_nodes;
        ib_free_nodes_counter = u_ib_nodes;
    }

    if (i < u_vb_pages) vb_state[i] = pack_state(0, ST_MERGED); // "ничего"
    if (i < u_ib_pages) ib_state[i] = pack_state(0, ST_MERGED);

    if (i < u_vb_nodes) vb_free_nodes_list[i] = i;
    if (i < u_ib_nodes) ib_free_nodes_list[i] = i;

    if (i < u_vb_heads_count) vb_heads[i] = pack_head(INVALID_HEAD_IDX, 0u);
    if (i < u_ib_heads_count) ib_heads[i] = pack_head(INVALID_HEAD_IDX, 0u);

    if (i < u_max_chunks) {
        chunk_alloc[i].v_startPage = INVALID_ID; 
        chunk_alloc[i].v_order = 0u; 
        chunk_alloc[i].needV = 0u; 
        chunk_alloc[i].i_startPage = INVALID_ID; 
        chunk_alloc[i].i_order = 0u; 
        chunk_alloc[i].needI = 0u; 
        chunk_alloc[i].need_rebuild = 0u;
    }
}
