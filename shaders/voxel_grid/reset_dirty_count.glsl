#version 430
layout(local_size_x = 1) in;

struct FrameCounters {
    uint write_count; 
    uint dirty_count;
    uint cmd_count;
    uint free_count;
    uint failed_dirty_count;
    uint count_vb_free_pages;
    uint count_ib_free_pages;
};
layout(std430, binding=0) buffer FrameCountersBuf { FrameCounters counters; };

void main() {
    counters.dirty_count = 0u;
}
