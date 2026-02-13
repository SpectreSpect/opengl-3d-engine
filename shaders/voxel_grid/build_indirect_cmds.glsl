#version 430
layout(local_size_x = 256) in;

layout(std430, binding=5) buffer FrameCounters { uvec4 counters; }; // z = cmdCount

struct ChunkMeta { uint used; uint key_lo; uint key_hi; uint dirty_flags; };
layout(std430, binding=6) readonly buffer ChunkMetaBuf { ChunkMeta meta[]; };

struct ChunkMeshMeta { uint first_index; uint index_count; uint base_vertex; uint mesh_valid; };
layout(std430, binding=9) readonly buffer ChunkMeshMetaBuf { ChunkMeshMeta mesh_meta[]; };

struct DrawElementsIndirectCommand {
    uint count;
    uint instanceCount;
    uint firstIndex;
    int  baseVertex;
    uint baseInstance;
};
layout(std430, binding=15) buffer IndirectCmdBuf { DrawElementsIndirectCommand cmds[]; };

uniform uint  u_max_chunks;

// для AABB/sphere размеров чанка
uniform ivec3 u_chunk_dim;     // (16,16,16)
uniform vec3  u_voxel_size;    // (sx,sy,sz)

// pack/unpack как в C++
uniform uint u_pack_bits;
uniform int  u_pack_offset;

// 6 плоскостей фрустума в world space: ax+by+cz+d >= 0 (внутри)
uniform vec4 u_frustum_planes[6];

uint mask_bits(uint bits) {
    return (bits >= 32u) ? 0xFFFFFFFFu : ((1u << bits) - 1u);
}

// извлекает `bits` начиная с `shift` из 64-битного значения, хранящегося как uvec2(lo,hi)
uint extract_bits_uvec2(uvec2 k, uint shift, uint bits) {
    uint m = mask_bits(bits);
    if (bits == 0u) return 0u;

    if (shift < 32u) {
        uint res = k.x >> shift;

        uint rem = 32u - shift;
        if (rem < bits) {
            res |= (k.y << rem);
        }
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

// Быстрый тест сферы против плоскостей (false positives допустимы)
bool sphere_in_frustum(vec3 center, float radius) {
    for (int i = 0; i < 6; ++i) {
        vec4 p = u_frustum_planes[i];
        float dist = dot(p.xyz, center) + p.w;
        if (dist < -radius) return false;
    }
    return true;
}

void main() {
    uint chunkId = gl_GlobalInvocationID.x;
    if (chunkId >= u_max_chunks) return;

    if (meta[chunkId].used == 0u) return;

    ChunkMeshMeta m = mesh_meta[chunkId];
    if (m.mesh_valid == 0u || m.index_count == 0u) return;

    ivec3 chunkCoord = unpack_key_to_coord(uvec2(meta[chunkId].key_lo, meta[chunkId].key_hi));

    vec3 chunkSize = vec3(u_chunk_dim) * u_voxel_size;

    vec3 minP = vec3(chunkCoord * u_chunk_dim) * u_voxel_size;
    vec3 center = minP + 0.5 * chunkSize;

    float radius = length(chunkSize) * 0.5;

    if (!sphere_in_frustum(center, radius)) return;

    uint cmdIdx = atomicAdd(counters.z, 1u);

    cmds[cmdIdx].count         = m.index_count;
    cmds[cmdIdx].instanceCount = 1u;
    cmds[cmdIdx].firstIndex    = m.first_index;
    cmds[cmdIdx].baseVertex    = 0;
    cmds[cmdIdx].baseInstance  = chunkId;
}
