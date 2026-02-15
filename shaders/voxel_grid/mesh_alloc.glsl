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

uniform uint u_vb_pages;
uniform uint u_ib_pages;
uniform uint u_vb_page_verts;  // например 256
uniform uint u_ib_page_inds;   // например 1024
uniform uint u_vb_max_order;   // log2(u_vb_pages)
uniform uint u_ib_max_order;   // log2(u_ib_pages)

// ---- state packing ----
const uint ST_FREE   = 0u;
const uint ST_ALLOC  = 1u;
const uint ST_LOCKED = 2u;
const uint ST_MASK   = 3u;

uint pack_state(uint order, uint kind) { return (order << 2u) | (kind & ST_MASK); }

uint div_up_u32(uint a, uint b) { return (a + b - 1u) / b; }

uint ceil_log2_u32(uint x) {
    if (x <= 1u) return 0u;
    // ceil_log2(x) = floor_log2(x-1) + 1
    return uint(findMSB(x - 1u) + 1);
}

void vb_push_free(uint order, uint startPage) {
    atomicExchange(vb_state[startPage], pack_state(order, ST_FREE));

    for (uint it = 0u; it < 64u; ++it) {
        uint old = atomicAdd(vb_heads[order], 0u);
        vb_next[startPage] = old;
        memoryBarrierBuffer(); // next видим до публикации head
        uint prev = atomicCompSwap(vb_heads[order], old, startPage);
        if (prev == old) return;
    }
}

uint vb_pop_free(uint order) {
    for (uint it = 0u; it < 128u; ++it) {
        uint h = atomicAdd(vb_heads[order], 0u);
        if (h == INVALID_ID) return INVALID_ID;

        memoryBarrierBuffer();
        uint nxt = vb_next[h];

        uint prev = atomicCompSwap(vb_heads[order], h, nxt);
        if (prev != h) continue;

        uint expected = pack_state(order, ST_FREE);
        uint got = atomicCompSwap(vb_state[h], expected, pack_state(order, ST_LOCKED));
        if (got == expected) return h; // success

        // stale узел (lazy delete) — игнорируем
    }
    return INVALID_ID;
}

uint vb_alloc_pages(uint wantOrder) {
    if (wantOrder > u_vb_max_order) return INVALID_ID;

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

void ib_push_free(uint order, uint startPage) {
    atomicExchange(ib_state[startPage], pack_state(order, ST_FREE));

    for (uint it = 0u; it < 64u; ++it) {
        uint old = atomicAdd(ib_heads[order], 0u);
        ib_next[startPage] = old;
        memoryBarrierBuffer();
        uint prev = atomicCompSwap(ib_heads[order], old, startPage);
        if (prev == old) return;
    }
}

uint ib_pop_free(uint order) {
    for (uint it = 0u; it < 128u; ++it) {
        uint h = atomicAdd(ib_heads[order], 0u);
        if (h == INVALID_ID) return INVALID_ID;

        memoryBarrierBuffer();
        uint nxt = ib_next[h];

        uint prev = atomicCompSwap(ib_heads[order], h, nxt);
        if (prev != h) continue;

        uint expected = pack_state(order, ST_FREE);
        uint got = atomicCompSwap(ib_state[h], expected, pack_state(order, ST_LOCKED));
        if (got == expected) return h;
    }
    return INVALID_ID;
}

uint ib_alloc_pages(uint wantOrder) {
    if (wantOrder > u_ib_max_order) return INVALID_ID;

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
    if (gl_GlobalInvocationID.x != 0u) return;
    
    // uint dirtyIdx = gl_GlobalInvocationID.x;
    uint dirtyCount = counters.y;
    // if (dirtyIdx >= dirtyCount) return;

    for (uint dirtyIdx = 0; dirtyIdx < dirtyCount; dirtyIdx++) {
        uint chunkId = dirty_list[dirtyIdx];

        // мог быть уже выселен
        if (meta[chunkId].used == 0u) {
            free_chunk_mesh(chunkId);
            mesh_meta[chunkId].mesh_valid  = 0u;
            mesh_meta[chunkId].index_count = 0u;
            emit_counter[chunkId] = 0u;
            enqueued[chunkId] = 0u;
            continue;
        }

        uint quads = dirty_quad_count[dirtyIdx];

        // пустой меш
        if (quads == 0u) {
            free_chunk_mesh(chunkId);
            mesh_meta[chunkId].mesh_valid  = 0u;
            mesh_meta[chunkId].index_count = 0u;
            emit_counter[chunkId] = 0u;
            enqueued[chunkId] = 0u;
            continue;
        }

        // free старого
        free_chunk_mesh(chunkId);

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
            continue;
        }

        uint iStart = ib_alloc_pages(iOrder);
        if (iStart == INVALID_ID) {
            vb_free_pages(vStart, vOrder); // rollback
            mesh_meta[chunkId].mesh_valid  = 0u;
            mesh_meta[chunkId].index_count = 0u;
            emit_counter[chunkId] = 0u;
            enqueued[chunkId] = 0u;
            continue;
        }

        chunk_alloc[chunkId] = uvec4(vStart, vOrder, iStart, iOrder);

        mesh_meta[chunkId].base_vertex = vStart * u_vb_page_verts;
        mesh_meta[chunkId].first_index = iStart * u_ib_page_inds;
        mesh_meta[chunkId].index_count = needI;
        mesh_meta[chunkId].mesh_valid  = 0u;

        emit_counter[chunkId] = 0u;
        enqueued[chunkId] = 0u;
    }
}
