#version 430
layout(local_size_x = 256) in;

#define SLOT_EMPTY  0xFFFFFFFFu
#define SLOT_LOCKED 0xFFFFFFFEu
#define SLOT_TOMB   0xFFFFFFFDu
#define INVALID_ID  0xFFFFFFFFu

#define MAX_PROBES  128u
#define LOCK_SPINS  64u

layout(std430, binding=0) buffer ChunkHashKeys { uvec2 hash_keys[]; };
layout(std430, binding=1) buffer ChunkHashVals { uint  hash_vals[]; };

layout(std430, binding=4) buffer FreeList { uint free_list[]; };
layout(std430, binding=5) buffer FrameCounters { uvec4 counters; }; // w=freeCount

struct ChunkMeta { uint used; uint key_lo; uint key_hi; uint dirty_flags; };
layout(std430, binding=6) buffer ChunkMetaBuf { ChunkMeta meta[]; };

layout(std430, binding=7) buffer EnqueuedBuf { uint enqueued[]; };

struct ChunkMeshMeta { uint first_index; uint index_count; uint base_vertex; uint mesh_valid; };
layout(std430, binding=9) buffer ChunkMeshMetaBuf { ChunkMeshMeta mesh_meta[]; };

// список выселяемых чанков
layout(std430, binding=16) readonly buffer EvictListBuf { uint evict_ids[]; };

uniform uint u_evict_count;
uniform uint u_hash_table_size;

const uint ST_FREE   = 0u;
const uint ST_ALLOC  = 1u;
const uint ST_LOCKED = 2u;
const uint ST_MASK   = 3u;

uint hash_uvec2(uvec2 v) {
    uint x = v.x * 1664525u + 1013904223u;
    uint y = v.y * 22695477u + 1u;
    uint h = x ^ (y + (x << 16) + (x >> 16));
    h ^= h >> 16; h *= 0x7feb352du;
    h ^= h >> 15; h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

bool remove_from_table(uvec2 key, uint chunkId) {
    uint mask = u_hash_table_size - 1u;
    uint idx  = hash_uvec2(key) & mask;

    for (uint probe = 0u; probe < MAX_PROBES; /* probe++ внутри */) {
        uint v = atomicAdd(hash_vals[idx], 0u);

        if (v == SLOT_LOCKED) {
            uint spins = 0u;
            while (spins++ < 1024u) {
                v = atomicAdd(hash_vals[idx], 0u);
                if (v != SLOT_LOCKED) break;
            }
            if (v == SLOT_LOCKED) return false; 
        }

        if (v == SLOT_EMPTY) {
            return false;
        }

        if (v == SLOT_TOMB) {
            // могила => продолжаем поиск
            idx = (idx + 1u) & mask;
            probe++;
            continue;
        }

        if (v == chunkId && all(equal(hash_keys[idx], key))) {
            uint prev = atomicCompSwap(hash_vals[idx], v, SLOT_TOMB);
            if (prev == v) {
                return true;
            }
            continue;
        }

        idx = (idx + 1u) & mask;
        probe++;
    }

    return false;
}

uint pack_state(uint order, uint kind) { return (order << 2u) | (kind & ST_MASK); }

// ---- intrusive push/pop with lazy deletion ----
void push_free(inout uint heads[], inout uint nexts[], inout uint state[], uint order, uint startPage) {
    // state -> FREE(order)
    atomicExchange(state[startPage], pack_state(order, ST_FREE));

    for (uint it = 0u; it < 64u; ++it) {
        uint old = atomicAdd(heads[order], 0u);
        nexts[startPage] = old;
        memoryBarrierBuffer(); // next visible before publishing head
        uint prev = atomicCompSwap(heads[order], old, startPage);
        if (prev == old) return;
    }
}

// free with merge; buddy claimed via state CAS; removal from list is lazy
void free_pages(inout uint heads[], inout uint nexts[], inout uint state[],
                uint start, uint order, uint maxOrder)
{
    if (start == INVALID_ID) return;

    // claim our block
    atomicExchange(state[start], pack_state(order, ST_LOCKED));

    uint s = start;
    uint o = order;

    for (uint it = 0u; it < 32u && o < maxOrder; ++it) {
        uint buddy = s ^ (1u << o);

        uint expected = pack_state(o, ST_FREE);
        uint got = atomicCompSwap(state[buddy], expected, pack_state(o, ST_LOCKED));
        if (got != expected) break; // buddy not free -> stop

        // merge
        s = min(s, buddy);
        o = o + 1u;

        // ensure merged start is locked (если merged start == buddy, он уже locked)
        atomicExchange(state[s], pack_state(o, ST_LOCKED));
    }

    // push merged block as free
    push_free(heads, nexts, state, o, s);
}

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= u_evict_count) return;

    uint chunkId = evict_ids[i];
    if (meta[chunkId].used == 0u) return;

    uvec2 key = uvec2(meta[chunkId].key_lo, meta[chunkId].key_hi);

    // 1) удалить из hash-table
    remove_from_table(key, chunkId);

    // 2) сбросить метаданные чанка
    meta[chunkId].used = 0u;
    meta[chunkId].dirty_flags = 0u;
    enqueued[chunkId] = 0u;

    mesh_meta[chunkId].mesh_valid = 0u;
    mesh_meta[chunkId].index_count = 0u;
    mesh_meta[chunkId].first_index = 0u;
    mesh_meta[chunkId].base_vertex = 0u;

    uvec4 a = chunk_alloc[victim];
    if (a.x != INVALID_ID) {
        free_pages(vb_heads, vb_next, vb_state, a.x, a.y, u_vb_max_order);
        free_pages(ib_heads, ib_next, ib_state, a.z, a.w, u_ib_max_order);
        chunk_alloc[victim] = uvec4(INVALID_ID,0u,INVALID_ID,0u);
    }

    // 3) вернуть chunkId в free_list
    uint slot = atomicAdd(counters.w, 1u);
    free_list[slot] = chunkId;
}
