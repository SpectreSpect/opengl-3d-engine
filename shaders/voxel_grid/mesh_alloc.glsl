#version 430
layout(local_size_x = 256) in;

layout(std430, binding=5) buffer FrameCounters { uvec4 counters; }; // y = dirtyCount
layout(std430, binding=8) readonly buffer DirtyListBuf { uint dirty_list[]; };
layout(std430, binding=11) readonly buffer DirtyQuadCountBuf { uint dirty_quad_count[]; };

layout(std430, binding=12) buffer EmitCounterBuf { uint emit_counter[]; };
layout(std430, binding=7) buffer EnqueuedBuf { uint enqueued[]; };

layout(std430, binding=10) buffer MeshCounters { uvec2 meshCounters; }; // x=vertexCounter, y=indexCounter

struct ChunkMeshMeta { uint first_index; uint index_count; uint base_vertex; uint mesh_valid; };
layout(std430, binding=9) buffer ChunkMeshMetaBuf { ChunkMeshMeta mesh_meta[]; };

uniform uint u_max_vertices;
uniform uint u_max_indices;

void main() {
    uint dirtyIdx = gl_GlobalInvocationID.x;
    uint dirtyCount = counters.y;
    if (dirtyIdx >= dirtyCount) return;

    uint chunkId = dirty_list[dirtyIdx];
    uint quads   = dirty_quad_count[dirtyIdx];

    uint needV = quads * 4u;
    uint needI = quads * 6u;

    uint baseV = atomicAdd(meshCounters.x, needV);
    uint baseI = atomicAdd(meshCounters.y, needI);

    // budget check
    if (baseV + needV > u_max_vertices || baseI + needI > u_max_indices) {
        // откатить атомики без CAS нельзя, поэтому просто делаем chunk invalid
        mesh_meta[chunkId].mesh_valid  = 0u;
        mesh_meta[chunkId].index_count = 0u;
        mesh_meta[chunkId].base_vertex = 0u;
        mesh_meta[chunkId].first_index = 0u;

        emit_counter[chunkId] = 0u;
        enqueued[chunkId] = 0u;
        return;
    }

    mesh_meta[chunkId].base_vertex = baseV;
    mesh_meta[chunkId].first_index = baseI;
    mesh_meta[chunkId].index_count = needI;
    mesh_meta[chunkId].mesh_valid  = 0u;

    emit_counter[chunkId] = 0u;
    enqueued[chunkId] = 0u; // можно снова добавлять в dirty_list в будущем
}
