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
layout(std430, binding=1) buffer VBReturnedNodesList  { uint vb_returned_nodes_counter; uint vb_returned_nodes_list[]; };
layout(std430, binding=2) buffer IBReturnedNodesList  { uint ib_returned_nodes_counter; uint ib_returned_nodes_list[]; };

void main() {
    counters.dirty_count = 0u;
    vb_returned_nodes_counter = 0u;
    ib_returned_nodes_counter = 0u;
}
