#version 430
layout(local_size_x = 256) in;

struct Node {uint page; uint next;};

// ---- VB pool ----
layout(std430, binding=0) buffer VBFreeNodesList  { uint vb_free_nodes_counter; uint vb_free_nodes_list[];  };
layout(std430, binding=1) buffer VBReturnedNodesList  { uint vb_returned_nodes_counter; uint vb_returned_nodes_list[]; };

// ---- IB pool ----
layout(std430, binding=2) buffer IBFreeNodesList  { uint ib_free_nodes_counter; uint ib_free_nodes_list[];  };
layout(std430, binding=3) buffer IBReturnedNodesList  { uint ib_returned_nodes_counter; uint ib_returned_nodes_list[]; };

struct FrameCounters {
    uint write_count; 
    uint dirty_count;
    uint cmd_count;
    uint free_count;
    uint failed_dirty_count;
    uint count_vb_free_pages;
    uint count_ib_free_pages;
};
layout(std430, binding=4) buffer FrameCountersBuf { FrameCounters counters; };

uniform uvec3 u3_chunk_size;

uint div_up_u32(uint a, uint b) { return (a + b - 1u) / b; }

uint max(uint a, uint b) {
    return a > b ? a : b;
}

void main() {
    uint returned_list_id = gl_GlobalInvocationID.x;

    if (returned_list_id < vb_returned_nodes_counter) {
        uint returned_node_id = vb_returned_nodes_list[returned_list_id];
        vb_free_nodes_list[vb_free_nodes_counter + returned_list_id] = returned_node_id;
    }

    if (returned_list_id < ib_returned_nodes_counter) {
        uint returned_node_id = ib_returned_nodes_list[returned_list_id];
        ib_free_nodes_list[ib_free_nodes_counter + returned_list_id] = returned_node_id;
    }
}
