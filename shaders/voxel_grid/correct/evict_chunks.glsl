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

    // 3) вернуть chunkId в free_list
    uint slot = atomicAdd(counters.w, 1u);
    free_list[slot] = chunkId;
}
