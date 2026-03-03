#version 430
layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

#define SLOT_EMPTY   0xFFFFFFFFu
#define SLOT_LOCKED  0xFFFFFFFEu
#define INVALID_ID   0xFFFFFFFFu
#define MAX_PROBES   128u
#define LOCK_SPINS   32u

// --- hash table ---
layout(std430, binding=0) coherent buffer ChunkHashKeys { uvec2 hash_keys[]; };
layout(std430, binding=1) coherent buffer ChunkHashVals { uint  hash_vals[]; };

// --- allocator ---
layout(std430, binding=4) buffer FreeList { uint free_list[]; };

struct FrameCounters {
    uint write_count; 
    uint dirty_count;
    uint cmd_count;
    uint free_count;
    uint failed_dirty_count;
    uint count_vb_free_pages;
    uint count_ib_free_pages;
};
layout(std430, binding=5) buffer FrameCountersBuf { FrameCounters counters; }; // w = freeCount

// --- chunk meta ---
struct ChunkMeta { uint used; uint key_lo; uint key_hi; uint dirty_flags; };
layout(std430, binding=6) buffer ChunkMetaBuf { ChunkMeta meta[]; };

// --- dirty enqueued marker (чтобы новый чанк точно не считался уже enqueued)
layout(std430, binding=7) buffer EnqueuedBuf { uint enqueued[]; };

// --- stream out ---
layout(std430, binding=16) buffer StreamCounters { uvec2 stream; }; // x = loadCount
layout(std430, binding=17) buffer LoadList { uint load_list[]; };

uniform uint  u_hash_table_size;   // pow2
uniform uint  u_max_load_entries;  // обычно = count_active_chunks
uniform ivec3 u_chunk_dim;
uniform vec3  u_voxel_size;

uniform vec3  u_cam_pos_local;    // камера в локальных координатах грида
uniform int   u_radius_chunks;    // R в чанках

uniform uint u_pack_bits;
uniform int  u_pack_offset;

// ---------------- hash ----------------
uint hash_uvec2(uvec2 v) {
    uint x = v.x * 1664525u + 1013904223u;
    uint y = v.y * 22695477u + 1u;
    uint h = x ^ (y + (x << 16) + (x >> 16));
    h ^= h >> 16; h *= 0x7feb352du;
    h ^= h >> 15; h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

// ---------------- pack uvec2 without uint64 ----------------
uint mask_bits(uint bits) { return (bits >= 32u) ? 0xFFFFFFFFu : ((1u << bits) - 1u); }

uvec2 pack_key_uvec2(ivec3 c) {
    uint B = u_pack_bits;
    uint m = mask_bits(B);

    uint ux = uint(c.x + u_pack_offset) & m;
    uint uy = uint(c.y + u_pack_offset) & m;
    uint uz = uint(c.z + u_pack_offset) & m;

    uint lo = 0u, hi = 0u;
    lo |= uz;

    // uy at shift B
    if (B < 32u) {
        lo |= (uy << B);
        if (B != 0u && (B + B) > 32u) hi |= (uy >> (32u - B));
    } else {
        hi |= (uy << (B - 32u));
    }

    // ux at shift 2B
    uint s = 2u * B;
    if (s < 32u) {
        lo |= (ux << s);
        if (B != 0u && (s + B) > 32u) hi |= (ux >> (32u - s));
    } else if (s == 32u) {
        hi |= ux;
    } else {
        hi |= (ux << (s - 32u));
    }

    return uvec2(lo, hi);
}

// ---------------- allocator ----------------
uint pop_free_chunk_id() {
    uint old = atomicAdd(counters.free_count, 0xFFFFFFFFu); // -1
    if (old == 0u) {
        atomicAdd(counters.free_count, 1u); // rollback
        return INVALID_ID;
    }
    return free_list[old - 1u];
}

// ---------------- get_or_create ----------------
bool get_or_create_chunk(uvec2 key, out uint outId, out bool created) {
    uint mask = u_hash_table_size - 1u;
    uint idx  = hash_uvec2(key) & mask;

    for (uint probe = 0u; probe < MAX_PROBES;) {
        uint v = atomicAdd(hash_vals[idx], 0u);

        if (v == SLOT_LOCKED) {
            uint spins = 0u;
            while (spins++ < LOCK_SPINS) {
                v = atomicAdd(hash_vals[idx], 0u);
                if (v != SLOT_LOCKED) break;
            }
            if (v == SLOT_LOCKED) return false; 
        }

        if (v == SLOT_EMPTY) {
            uint prev = atomicCompSwap(hash_vals[idx], SLOT_EMPTY, SLOT_LOCKED);
            if (prev != SLOT_EMPTY) {
                // кто-то успел — перепроверяем этот же idx
                continue;
            }

            uint id = pop_free_chunk_id();
            if (id == INVALID_ID) {
                atomicExchange(hash_vals[idx], SLOT_EMPTY);
                return false;
            }

            // meta подготовим ДО публикации id
            meta[id].used       = 1u;
            meta[id].key_lo     = key.x;
            meta[id].key_hi     = key.y;
            meta[id].dirty_flags= 0u;
            enqueued[id]        = 0u;

            // публикуем key
            hash_keys[idx] = key;

            // гарантируем, что key/meta видимы до публикации id
            memoryBarrierBuffer();

            // публикуем id (и одновременно "анлочим")
            atomicExchange(hash_vals[idx], id);

            outId = id;
            created = true;
            return true;
        }

        if (all(equal(hash_keys[idx], key))) {
            outId = v;
            created = false;
            return true;
        }

        idx = (idx + 1u) & mask;
        probe++;
    }

    return false;
}

void main() {
    uvec3 gid = gl_GlobalInvocationID.xyz;

    int R = u_radius_chunks;
    uint side = uint(2 * R + 1);

    if (gid.x >= side || gid.y >= side || gid.z >= side) return;

    ivec3 off = ivec3(gid) - ivec3(R);
    int d2 = off.x*off.x + off.y*off.y + off.z*off.z;
    if (d2 > R*R) return;
    // if (d2 > 4) return;

    vec3 chunkWorldSize = vec3(u_chunk_dim) * u_voxel_size;
    ivec3 camChunk = ivec3(floor(u_cam_pos_local / chunkWorldSize));

    ivec3 chunkCoord = camChunk + off;
    uvec2 key = pack_key_uvec2(chunkCoord);

    uint chunkId;
    bool created;

    if (!get_or_create_chunk(key, chunkId, created)) return;

    if (created) {
        uint i = atomicAdd(stream.x, 1u);
        if (i < u_max_load_entries) load_list[i] = chunkId;
    }
}
