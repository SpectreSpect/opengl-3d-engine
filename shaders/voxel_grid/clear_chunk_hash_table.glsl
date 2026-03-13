#version 430
layout(local_size_x = 256) in;

// --- hash table ---
layout(std430, binding=0) buffer ChunkHashKeys { uvec2 hash_keys[]; };
layout(std430, binding=1) buffer ChunkHashVals { uint count_tomb; uint  hash_vals[]; };

uniform uint u_hash_table_size;

// ----- include -----

#define NOT_INCLUDE_GET_OR_CREATE
#define NOT_INCLUDE_LOOKUP_REMOVE
#include "common/hash_table.glsl"
// -------------------

void main() {
    uint slot_id = gl_GlobalInvocationID.x;
    if (slot_id >= u_hash_table_size) return;

    hash_keys[slot_id] = uvec2(0u, 0u);
    hash_vals[slot_id] = SLOT_EMPTY;
}