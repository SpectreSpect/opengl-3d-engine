#version 430

#define INVALID_ID 0xFFFFFFFFu

layout(local_size_x = 256) in;

struct FrameCounters {
    uint write_count; 
    uint dirty_count;
    uint cmd_count;
    uint free_count;
    uint failed_dirty_count;
    uint count_vb_free_pages;
    uint count_ib_free_pages;
};
layout(std430, binding=5) buffer FrameCountersBuf { FrameCounters counters; }; // z = cmdCount

struct ChunkMeta { uint used; uint key_lo; uint key_hi; uint dirty_flags; };
layout(std430, binding=6) readonly buffer ChunkMetaBuf { ChunkMeta meta[]; };

// x=v_startPage, y=v_order, z=i_startPage, w=i_order
struct ChunkMeshAlloc {uint v_startPage; uint v_order; uint needV; uint i_startPage; uint i_order; uint needI; uint need_rebuild; };
layout(std430, binding=24) buffer ChunkMeshAllocBuf { ChunkMeshAlloc chunk_mesh_alloc[]; }; 

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

uniform uint u_vb_page_verts;
uniform uint u_ib_page_inds;

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

    ChunkMeshAlloc mesh_alloc = chunk_mesh_alloc[chunkId];
    // {uint v_startPage; uint v_order; uint i_startPage; uint i_order; };
    if (mesh_alloc.v_startPage == INVALID_ID || mesh_alloc.i_startPage == INVALID_ID) return;
    if (mesh_alloc.needV == 0 || mesh_alloc.needI == 0) return;

    // ChunkMeshMeta m = mesh_meta[chunkId];
    // if (m.mesh_valid == 0u || m.index_count == 0u) return;

    ivec3 chunkCoord = unpack_key_to_coord(uvec2(meta[chunkId].key_lo, meta[chunkId].key_hi));

    vec3 chunkSize = vec3(u_chunk_dim) * u_voxel_size;

    vec3 minP = vec3(chunkCoord * u_chunk_dim) * u_voxel_size;
    vec3 center = minP + 0.5 * chunkSize;

    float radius = length(chunkSize) * 0.5;

    if (!sphere_in_frustum(center, radius)) return;

    uint cmdIdx = atomicAdd(counters.cmd_count, 1u);

    cmds[cmdIdx].count         = mesh_alloc.needI;
    cmds[cmdIdx].instanceCount = 1u;
    cmds[cmdIdx].firstIndex    = mesh_alloc.i_startPage * u_ib_page_inds;
    cmds[cmdIdx].baseVertex    = 0;
    cmds[cmdIdx].baseInstance  = chunkId;
}
