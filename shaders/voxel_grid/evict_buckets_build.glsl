#version 430
layout(local_size_x = 256) in;

#define INVALID_ID 0xFFFFFFFFu

struct ChunkMeta { uint used; uint key_lo; uint key_hi; uint dirty_flags; };
layout(std430, binding=6) readonly buffer ChunkMetaBuf { ChunkMeta meta[]; };

layout(std430, binding=16) coherent buffer BucketHeads { uint bucket_heads[]; };
layout(std430, binding=17) coherent buffer BucketNext  { uint bucket_next[]; };

uniform uint  u_max_chunks;
uniform uint  u_bucket_count;

uniform vec3  u_cam_pos;
uniform ivec3 u_chunk_dim;
uniform vec3  u_voxel_size;

// как у тебя в других шейдерах:
uniform uint u_pack_bits;
uniform int  u_pack_offset;

// dist2 * scale -> bucket
// Пример: bucket_scale = 1 / (step*step), step = 32м -> scale ~ 0.0009765625
uniform float u_bucket_scale;

// ---- helpers unpack uvec2 -> ivec3 ----
uint mask_bits(uint bits) {
    return (bits >= 32u) ? 0xFFFFFFFFu : ((1u << bits) - 1u);
}

uint extract_bits_uvec2(uvec2 k, uint shift, uint bits) {
    uint m = mask_bits(bits);
    if (bits == 0u) return 0u;

    if (shift < 32u) {
        uint lo_part = k.x >> shift;
        uint res = lo_part;
        uint rem = 32u - shift;
        if (rem < bits) res |= (k.y << rem);
        return res & m;
    } else {
        uint s = shift - 32u;
        return (k.y >> s) & m;
    }
}

ivec3 unpack_key_to_coord(uvec2 key2) {
    uint B = u_pack_bits;
    uint ux = extract_bits_uvec2(key2, 2u * B, B);
    uint uy = extract_bits_uvec2(key2, 1u * B, B);
    uint uz = extract_bits_uvec2(key2, 0u * B, B);
    return ivec3(int(ux) - u_pack_offset,
                 int(uy) - u_pack_offset,
                 int(uz) - u_pack_offset);
}

// push chunkId into bucket list (head = chunkId, next[chunkId] = oldHead)
void push_bucket(uint b, uint chunkId) {
    for (;;) {
        uint old = atomicAdd(bucket_heads[b], 0u);
        bucket_next[chunkId] = old;
        memoryBarrierBuffer(); // next должен стать видим до публикации head
        uint prev = atomicCompSwap(bucket_heads[b], old, chunkId);
        if (prev == old) break;
    }
}

uint bucket_for_coord(ivec3 chunkCoord) {
    vec3 chunkSize = vec3(u_chunk_dim) * u_voxel_size;
    vec3 minP = vec3(chunkCoord * u_chunk_dim) * u_voxel_size;
    vec3 center = minP + 0.5 * chunkSize;

    vec3 d = center - u_cam_pos;
    float dist2 = dot(d, d);

    uint b = uint(dist2 * u_bucket_scale);
    if (b >= u_bucket_count) b = u_bucket_count - 1u;
    return b;
}

void main() {
    uint chunkId = gl_GlobalInvocationID.x;
    if (chunkId >= u_max_chunks) return;
    if (meta[chunkId].used == 0u) return;

    uvec2 key2 = uvec2(meta[chunkId].key_lo, meta[chunkId].key_hi);
    ivec3 cc = unpack_key_to_coord(key2);

    uint b = bucket_for_coord(cc);
    push_bucket(b, chunkId);
}
