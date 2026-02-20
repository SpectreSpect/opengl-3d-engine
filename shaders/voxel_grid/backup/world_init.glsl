#version 430
layout(local_size_x = 256) in;

#define SLOT_EMPTY 0xFFFFFFFFu
#define SLOT_TOMB  0xFFFFFFFDu

layout(std430, binding=0) coherent buffer ChunkHashKeys { uvec2 hash_keys[]; };
layout(std430, binding=1) coherent buffer ChunkHashVals { uint  hash_vals[]; };

layout(std430, binding=4) buffer FreeList { uint free_list[]; };

struct FrameCounters {uint write_count; uint dirty_count; uint cmd_count; uint free_count; uint failed_dirty_count; };
layout(std430, binding=5) buffer FrameCountersBuf { FrameCounters counters; };

struct ChunkMeta { uint used; uint key_lo; uint key_hi; uint dirty_flags; };
layout(std430, binding=6) buffer ChunkMetaBuf { ChunkMeta meta[]; };

layout(std430, binding=7) buffer EnqueuedBuf { uint enqueued[]; };

layout(std430, binding=8) buffer CountFreePagesBuf { uvec2 count_free_pages; };

uniform uint u_hash_table_size;
uniform uint u_max_chunks;
uniform uint u_count_vb_pages;
uniform uint u_count_ib_pages;

void main() {
    uint i = gl_GlobalInvocationID.x;

    if (i < u_hash_table_size) {
        hash_vals[i] = SLOT_EMPTY;
        hash_keys[i] = uvec2(0u, 0u);
    }

    if (i < u_max_chunks) {
        free_list[i]   = i;
        enqueued[i]    = 0u;

        meta[i].used       = 0u;
        meta[i].key_lo     = 0u;
        meta[i].key_hi     = 0u;
        meta[i].dirty_flags= 0u;
    }

    if (i == 0u) {
        counters.write_count = 0u; 
        counters.dirty_count = 0u; 
        counters.cmd_count = 0u; 
        counters.free_count = u_max_chunks; 
        counters.failed_dirty_count = 0u;
        
        count_free_pages.x = u_count_vb_pages;
        count_free_pages.y = u_count_ib_pages;
    }
}
