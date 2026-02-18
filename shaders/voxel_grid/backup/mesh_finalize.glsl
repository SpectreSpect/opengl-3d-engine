#version 430
layout(local_size_x = 256) in;

layout(std430, binding=5) buffer FrameCounters { uvec4 counters; }; // y = dirtyCount
layout(std430, binding=8) readonly buffer DirtyListBuf { uint dirty_list[]; };

layout(std430, binding=7) buffer EnqueuedBuf { uint enqueued[]; };

struct ChunkMeta { uint used; uint key_lo; uint key_hi; uint dirty_flags; };
layout(std430, binding=6) buffer ChunkMetaBuf { ChunkMeta meta[]; };

struct ChunkMeshMeta { uint first_index; uint index_count; uint base_vertex; uint mesh_valid; };
layout(std430, binding=9) buffer ChunkMeshMetaBuf { ChunkMeshMeta mesh_meta[]; };

uniform uint u_dirty_flag_bits; // например 1u

void main() {
    uint dirtyIdx = gl_GlobalInvocationID.x;
    uint dirtyCount = counters.y;
    if (dirtyIdx >= dirtyCount) return;

    uint chunkId = dirty_list[dirtyIdx];

    // меш построен
    mesh_meta[chunkId].mesh_valid = (mesh_meta[chunkId].index_count != 0u) ? 1u : 0u;

    // снять dirty флаг(и)
    meta[chunkId].dirty_flags &= ~u_dirty_flag_bits;

    // разрешить повторно enqueue
    enqueued[chunkId] = 0u;
}
