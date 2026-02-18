#version 430
layout(local_size_x = 256) in;

#define INVALID_ID 0xFFFFFFFFu

// ---- existing ----
layout(std430, binding=5) buffer FrameCounters { uvec4 counters; }; // y = dirtyCount
layout(std430, binding=8) readonly buffer DirtyListBuf { uint dirty_list[]; };
layout(std430, binding=11) readonly buffer DirtyQuadCountBuf { uint dirty_quad_count[]; };

layout(std430, binding=12) buffer EmitCounterBuf { uint emit_counter[]; };
layout(std430, binding=7)  buffer EnqueuedBuf    { uint enqueued[]; };

struct ChunkMeta { uint used; uint key_lo; uint key_hi; uint dirty_flags; };
layout(std430, binding=6) readonly buffer ChunkMetaBuf { ChunkMeta meta[]; };

struct ChunkMeshMeta { uint first_index; uint index_count; uint base_vertex; uint mesh_valid; };
layout(std430, binding=9) buffer ChunkMeshMetaBuf { ChunkMeshMeta mesh_meta[]; };

// ---- NEW: per-chunk alloc info ----
layout(std430, binding=24) buffer ChunkMeshAllocBuf { uvec4 chunk_alloc[]; }; 
// x=v_startPage, y=v_order, z=i_startPage, w=i_order

// ---- NEW: VB pool ----
layout(std430, binding=18) coherent buffer VBHeads { uint vb_heads[]; };
layout(std430, binding=19) coherent buffer VBNext  { uint vb_next[];  };
layout(std430, binding=20) coherent buffer VBState { uint vb_state[]; };

// ---- NEW: IB pool ----
layout(std430, binding=21) coherent buffer IBHeads { uint ib_heads[]; };
layout(std430, binding=22) coherent buffer IBNext  { uint ib_next[];  };
layout(std430, binding=23) coherent buffer IBState { uint ib_state[]; };

layout(std430, binding=25) buffer DebugBuffer { uint stats[]; };

uniform uint u_vb_pages;
uniform uint u_ib_pages;
uniform uint u_vb_page_verts;  // например 256
uniform uint u_ib_page_inds;   // например 1024
uniform uint u_vb_max_order;   // log2(u_vb_pages)
uniform uint u_ib_max_order;   // log2(u_ib_pages)
uniform uint u_vb_index_bits;
uniform uint u_ib_index_bits;

// ---- state packing ----
const uint ST_MASK_BITS = 4;
const uint ST_FREE    = 0u;
const uint ST_ALLOC   = 1u;
const uint ST_LOCKED  = 2u;
const uint ST_PUSHING = 3u;
const uint ST_MASK   = (1 << ST_MASK_BITS) - 1;

uint pack_state(uint order, uint kind) { return (order << ST_MASK_BITS) | (kind & ST_MASK); }

uint div_up_u32(uint a, uint b) { return (a + b - 1u) / b; }

uint ceil_log2_u32(uint x) {
    if (x <= 1u) return 0u;
    // ceil_log2(x) = floor_log2(x-1) + 1
    return uint(findMSB(x - 1u) + 1);
}

uint vb_mask() { return (1u << u_vb_index_bits) - 1u; }

uint vb_pack_head(uint idx, uint tag) { return (tag << u_vb_index_bits) | idx; }
uint vb_head_idx(uint h) { return h & vb_mask(); }
uint vb_head_tag(uint h) { return h >> u_vb_index_bits; }

void vb_push_free(uint order, uint startPage) {
    // важный инвариант: в списке лежат только FREE узлы
    // while(true) {
    //     uint was = atomicAdd(vb_state[startPage], 0u);
    //     if (was == ST_PUSHING) continue;
    //     if (was == ST_FREE) return;
        
    //     atomicExchange(vb_state[startPage], pack_state(order, ST_PUSHING));
    //     memoryBarrierBuffer(); // state виден до публикации в head
    //     break;
    // }

    atomicExchange(vb_state[startPage], pack_state(order, ST_PUSHING));
    memoryBarrierBuffer(); // state виден до публикации в head

    for (;;) {
        uint oldH   = atomicAdd(vb_heads[order], 0u);
        uint oldIdx = vb_head_idx(oldH);
        uint oldTag = vb_head_tag(oldH);

        vb_next[startPage] = oldIdx;
        memoryBarrierBuffer(); // next виден до публикации head

        uint newH = vb_pack_head(startPage, oldTag + 1u);
        if (atomicCompSwap(vb_heads[order], oldH, newH) == oldH) break;
    }

    memoryBarrierBuffer();
    atomicCompSwap(vb_state[startPage], pack_state(order, ST_PUSHING), pack_state(order, ST_FREE));
}

uint vb_pop_free(uint order) {
    uint invalid = vb_mask();

    for (uint it = 0u; it < 4096u; ++it) {
        uint oldH = atomicAdd(vb_heads[order], 0u);
        uint h    = vb_head_idx(oldH);
        uint tag  = vb_head_tag(oldH);

        if (h == invalid || h >= u_vb_pages) return INVALID_ID;

        memoryBarrierBuffer();
        uint nxt = vb_next[h];
        if (nxt != invalid && nxt >= u_vb_pages) nxt = invalid;

        uint newH = vb_pack_head(nxt, tag + 1u);
        if (atomicCompSwap(vb_heads[order], oldH, newH) != oldH) continue;

        // Мы сняли голову. Теперь либо забираем, либо ВЫКИДЫВАЕМ как stale.
        uint expected_free = pack_state(order, ST_FREE);
        uint expected_pushing = pack_state(order, ST_PUSHING);
        if (atomicCompSwap(vb_state[h], expected_free, pack_state(order, ST_LOCKED)) == expected_free ||
            atomicCompSwap(vb_state[h], expected_pushing, pack_state(order, ST_LOCKED)) == expected_pushing) {
            return h; // успех
        }

        // иначе это stale/corruption/coalesce-locked — просто выбросили узел из списка и идём дальше
        // (можно stats++)
    }

    return INVALID_ID;
}


uint vb_alloc_pages(uint wantOrder) {
    if (wantOrder > u_vb_max_order)  {
        atomicAdd(stats[5], 1u);
        return INVALID_ID;
    }

    for (uint attempt = 0u; attempt < 128u; ++attempt) {
        for (uint o = wantOrder; o <= u_vb_max_order; ++o) {
            uint start = vb_pop_free(o);
            if (start == INVALID_ID) continue;

            uint cur = o;
            while (cur > wantOrder) {
                cur--;

                uint halfPages = (1u << cur);
                uint buddy = start + halfPages;
                if (buddy >= u_vb_pages) break; // safety

                vb_push_free(cur, buddy);
                atomicExchange(vb_state[start], pack_state(cur, ST_LOCKED));
            }

            atomicExchange(vb_state[start], pack_state(wantOrder, ST_ALLOC));
            return start;
        }

        memoryBarrierBuffer();
    }

    atomicAdd(stats[6], 1u);
    return INVALID_ID;
}

void vb_free_pages(uint start, uint order) {
    if (start == INVALID_ID) return;

    atomicExchange(vb_state[start], pack_state(order, ST_LOCKED));

    uint s = start;
    uint o = order;

    for (uint it = 0u; it < 32u && o < u_vb_max_order; ++it) {
        uint buddy = s ^ (1u << o);

        // buddy должен быть стартом блока этого order и свободным
        uint expected = pack_state(o, ST_FREE);
        uint got = atomicCompSwap(vb_state[buddy], expected, pack_state(o, ST_LOCKED));
        if (got != expected) break;

        s = min(s, buddy);
        o = o + 1u;
        atomicExchange(vb_state[s], pack_state(o, ST_LOCKED));
    }

    vb_push_free(o, s);
}

uint ib_mask() { return (1u << u_ib_index_bits) - 1u; }

uint ib_pack_head(uint idx, uint tag) { return (tag << u_ib_index_bits) | idx; }
uint ib_head_idx(uint h) { return h & ib_mask(); }
uint ib_head_tag(uint h) { return h >> u_ib_index_bits; }

void ib_push_free(uint order, uint startPage) {
    // важный инвариант: в списке лежат только FREE узлы
    // while(true) {
    //     uint was = atomicAdd(ib_state[startPage], 0u);
    //     if (was == ST_PUSHING) continue;
    //     if (was == ST_FREE) return;
        
    //     atomicExchange(ib_state[startPage], pack_state(order, ST_PUSHING));
    //     memoryBarrierBuffer(); // state виден до публикации в head
    //     break;
    // }

    atomicExchange(ib_state[startPage], pack_state(order, ST_PUSHING));
    memoryBarrierBuffer(); // state виден до публикации в head

    for (;;) {
        uint oldH   = atomicAdd(ib_heads[order], 0u);
        uint oldIdx = ib_head_idx(oldH);
        uint oldTag = ib_head_tag(oldH);

        ib_next[startPage] = oldIdx;
        memoryBarrierBuffer(); // next виден до публикации head

        uint newH = ib_pack_head(startPage, oldTag + 1u);
        if (atomicCompSwap(ib_heads[order], oldH, newH) == oldH) break;
    }

    memoryBarrierBuffer();
    atomicCompSwap(ib_state[startPage], pack_state(order, ST_PUSHING), pack_state(order, ST_FREE));
}

uint ib_pop_free(uint order) {
    uint invalid = ib_mask();

    for (uint it = 0u; it < 4096u; ++it) {
        uint oldH = atomicAdd(ib_heads[order], 0u);
        uint h    = ib_head_idx(oldH);
        uint tag  = ib_head_tag(oldH);

        if (h == invalid || h >= u_ib_pages) return INVALID_ID;

        memoryBarrierBuffer();
        uint nxt = ib_next[h];
        if (nxt != invalid && nxt >= u_ib_pages) nxt = invalid;

        uint newH = ib_pack_head(nxt, tag + 1u);
        if (atomicCompSwap(ib_heads[order], oldH, newH) != oldH) continue;

        // Мы сняли голову. Теперь либо забираем, либо ВЫКИДЫВАЕМ как stale.
        uint expected_free = pack_state(order, ST_FREE);
        uint expected_pushing = pack_state(order, ST_PUSHING);
        if (atomicCompSwap(ib_state[h], expected_free, pack_state(order, ST_LOCKED)) == expected_free ||
            atomicCompSwap(ib_state[h], expected_pushing, pack_state(order, ST_LOCKED)) == expected_pushing) {
            return h; // успех
        }

        // иначе это stale/corruption/coalesce-locked — просто выбросили узел из списка и идём дальше
        // (можно stats++)
    }

    return INVALID_ID;
}

uint ib_alloc_pages(uint wantOrder) {
    if (wantOrder > u_ib_max_order) return INVALID_ID;

    for (uint attempt = 0u; attempt < 128u; ++attempt) {
        for (uint o = wantOrder; o <= u_ib_max_order; ++o) {
            uint start = ib_pop_free(o);
            if (start == INVALID_ID) continue;

            uint cur = o;
            while (cur > wantOrder) {
                cur--;

                uint halfPages = (1u << cur);
                uint buddy = start + halfPages;
                if (buddy >= u_ib_pages) break;

                ib_push_free(cur, buddy);
                atomicExchange(ib_state[start], pack_state(cur, ST_LOCKED));
            }

            atomicExchange(ib_state[start], pack_state(wantOrder, ST_ALLOC));
            return start;
        }
        
        memoryBarrierBuffer();
    }
    return INVALID_ID;
}

void ib_free_pages(uint start, uint order) {
    if (start == INVALID_ID) return;

    atomicExchange(ib_state[start], pack_state(order, ST_LOCKED));

    uint s = start;
    uint o = order;

    for (uint it = 0u; it < 32u && o < u_ib_max_order; ++it) {
        uint buddy = s ^ (1u << o);

        uint expected = pack_state(o, ST_FREE);
        uint got = atomicCompSwap(ib_state[buddy], expected, pack_state(o, ST_LOCKED));
        if (got != expected) break;

        s = min(s, buddy);
        o = o + 1u;
        atomicExchange(ib_state[s], pack_state(o, ST_LOCKED));
    }

    ib_push_free(o, s);
}

void free_chunk_mesh(uint chunkId) {
    uvec4 a = chunk_alloc[chunkId];
    if (a.x != INVALID_ID) vb_free_pages(a.x, a.y);
    if (a.z != INVALID_ID) ib_free_pages(a.z, a.w);
    chunk_alloc[chunkId] = uvec4(INVALID_ID, 0u, INVALID_ID, 0u);
}

void main() {
    // if (gl_GlobalInvocationID.x != 0u) return;
    
    uint dirtyIdx = gl_GlobalInvocationID.x;
    uint dirtyCount = counters.y;
    if (dirtyIdx >= dirtyCount) return;

    // for (uint dirtyIdx = 0; dirtyIdx < dirtyCount; dirtyIdx++) {
    uint chunkId = dirty_list[dirtyIdx];

    // мог быть уже выселен
    if (meta[chunkId].used == 0u) {
        // free_chunk_mesh(chunkId);
        mesh_meta[chunkId].mesh_valid  = 0u;
        mesh_meta[chunkId].index_count = 0u;
        emit_counter[chunkId] = 0u;
        enqueued[chunkId] = 0u;
        atomicAdd(stats[0], 1u);
        return;
    }

    uint quads = dirty_quad_count[dirtyIdx];

    // пустой меш
    if (quads == 0u) {
        // free_chunk_mesh(chunkId);
        mesh_meta[chunkId].mesh_valid  = 0u;
        mesh_meta[chunkId].index_count = 0u;
        emit_counter[chunkId] = 0u;
        enqueued[chunkId] = 0u;
        atomicAdd(stats[1], 1u);
        return;
    }

    // free старого
    // free_chunk_mesh(chunkId);

    uint needV = quads * 4u;
    uint needI = quads * 6u;

    uint vPages = div_up_u32(needV, u_vb_page_verts);
    uint iPages = div_up_u32(needI, u_ib_page_inds);

    uint vOrder = ceil_log2_u32(vPages);
    uint iOrder = ceil_log2_u32(iPages);

    uint vStart = vb_alloc_pages(vOrder);
    if (vStart == INVALID_ID) {
        mesh_meta[chunkId].mesh_valid  = 0u;
        mesh_meta[chunkId].index_count = 0u;
        emit_counter[chunkId] = 0u;
        enqueued[chunkId] = 0u;
        atomicAdd(stats[2], 1u);
        return;
    }

    uint iStart = ib_alloc_pages(iOrder);
    if (iStart == INVALID_ID) {
        // vb_free_pages(vStart, vOrder); // rollback
        mesh_meta[chunkId].mesh_valid  = 0u;
        mesh_meta[chunkId].index_count = 0u;
        emit_counter[chunkId] = 0u;
        enqueued[chunkId] = 0u;
        atomicAdd(stats[3], 1u);
        return;
    }

    chunk_alloc[chunkId] = uvec4(vStart, vOrder, iStart, iOrder);

    mesh_meta[chunkId].base_vertex = vStart * u_vb_page_verts;
    mesh_meta[chunkId].first_index = iStart * u_ib_page_inds;
    mesh_meta[chunkId].index_count = needI;
    mesh_meta[chunkId].mesh_valid  = 0u;

    emit_counter[chunkId] = 0u;
    enqueued[chunkId] = 0u;

    atomicAdd(stats[4], 1u);
    // }
}
