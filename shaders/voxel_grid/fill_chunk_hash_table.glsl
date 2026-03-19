#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) coherent buffer ChunkHashKeys { uvec2 hash_keys[]; };
layout(std430, binding=1) coherent buffer ChunkHashVals { uint count_tomb; uint  hash_vals[]; };
layout(std430, binding=2) buffer ChunkMetaBuf { ChunkMeta meta[]; };
layout(std430, binding=3) buffer EnqueuedBuf { uint enqueued[]; };

uniform uint u_max_chunks;
uniform uint u_hash_table_size;
uniform uint u_pack_bits;
uniform int u_pack_offset;

// ----- include -----
#include "../utils.glsl"

#define NOT_INCLUDE_GET_OR_CREATE
#include "../common/hash_table.glsl"
// -------------------

void main() {
    uint chunk_id = gl_GlobalInvocationID.x;
    if (chunk_id >= u_max_chunks) return;

    ChunkMeta chunk_meta = meta[chunk_id];
    if (chunk_meta.used == 0u) return;

    uvec2 key = uvec2(chunk_meta.key_lo, chunk_meta.key_hi);

    set_chunk(key, chunk_id);
}