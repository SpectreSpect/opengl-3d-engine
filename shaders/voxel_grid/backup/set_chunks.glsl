#version 430
layout(local_size_x = 256) in;

layout(std430, binding=0) coherent buffer ChunkHashKeys { uvec2 chunk_hash_keys[]; };
layout(std430, binding=1) coherent buffer ChunkHashVals { uint  chunk_hash_vals[]; };
layout(std430, binding=2) readonly buffer ChunkIndicesToSet { uint chunk_indices_to_set[]; };
layout(std430, binding=3) readonly buffer CoordKeysToSet { uvec2 coord_keys_to_set[]; };

uniform uint u_count_chunks_to_set;
uniform uint u_hash_table_size;
uniform uint u_max_count_probing;
uniform uint u_set_with_replace;

#define SLOT_EMPTY  0xFFFFFFFFu
#define SLOT_LOCKED 0xFFFFFFFEu
#define SLOT_TOMB   0xFFFFFFFDu
#define COUNT_RETRIES 5u

uint hash_uvec2(uvec2 v) {
    uint x = v.x * 1664525u + 1013904223u;
    uint y = v.y * 22695477u + 1u;
    uint h = x ^ (y + (x << 16) + (x >> 16));
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

void main() {
    uint list_idx = gl_GlobalInvocationID.x;
    if (list_idx >= u_count_chunks_to_set) return;

    uvec2 coord_key = coord_keys_to_set[list_idx];
    uint  chunk_id  = chunk_indices_to_set[list_idx];

    uint mask = u_hash_table_size - 1u;
    uint idx  = hash_uvec2(coord_key) & mask;

    for (uint i = 0u; i < u_max_count_probing; ++i) {
        uint slot_value = chunk_hash_vals[idx];

        if (slot_value == SLOT_EMPTY) {
            uint prev = atomicCompSwap(chunk_hash_vals[idx], SLOT_EMPTY, SLOT_LOCKED);
            if (prev == SLOT_EMPTY) {
                chunk_hash_keys[idx] = coord_key;
                memoryBarrierBuffer();
                chunk_hash_vals[idx] = chunk_id;
                return;
            }
        }
        else if (slot_value == SLOT_LOCKED) {
            uint spins = 0u;
            while (spins < COUNT_RETRIES && chunk_hash_vals[idx] == SLOT_LOCKED) {
                spins++;
            }
            continue;
        }
        else {
            if (all(equal(chunk_hash_keys[idx], coord_key))) {
                if (u_set_with_replace == 1u) {
                    atomicExchange(chunk_hash_vals[idx], chunk_id);
                }
                return;
            }
        }

        idx = (idx + 1u) & mask;
    }
}
