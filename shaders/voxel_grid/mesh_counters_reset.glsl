#version 430
layout(local_size_x = 1) in;

layout(std430, binding=0) buffer MeshCounters { uvec2 meshCounters; };

struct FrameCounters {
    uint write_count; 
    uint dirty_count;
    uint cmd_count;
    uint free_count;
    uint failed_dirty_count;
    uint count_vb_free_pages;
    uint count_ib_free_pages;
};
layout(std430, binding=1) buffer FrameCountersBuf { FrameCounters counters; };

void main() {
    meshCounters = uvec2(0u, 0u);
}
