#version 430
layout(local_size_x = 256) in;

struct Node {uint page; uint next;};

// ---- VB pool ----
layout(std430, binding=0) buffer VBFreeNodesList  { uint vb_free_nodes_counter; uint vb_free_nodes_list[];  };
layout(std430, binding=1) buffer VBReturnedNodesList  { uint vb_returned_nodes_counter; uint vb_returned_nodes_list[]; };

// ---- IB pool ----
layout(std430, binding=2) buffer IBFreeNodesList  { uint ib_free_nodes_counter; uint ib_free_nodes_list[];  };
layout(std430, binding=3) buffer IBReturnedNodesList  { uint ib_returned_nodes_counter; uint ib_returned_nodes_list[]; };

void vb_push_free_node_id(uint node_id) {
    uint free_idx = atomicAdd(vb_free_nodes_counter, 1u);
    vb_free_nodes_list[free_idx] = node_id;
}

void ib_push_free_node_id(uint node_id) {
    uint free_idx = atomicAdd(ib_free_nodes_counter, 1u);
    ib_free_nodes_list[free_idx] = node_id;
}

void main() {
    uint returned_list_id = gl_GlobalInvocationID.x;

    if (returned_list_id < vb_returned_nodes_counter) {
        uint returned_node_id = vb_returned_nodes_list[returned_list_id];
        vb_push_free_node_id(returned_node_id);

        vb_returned_nodes_list[returned_list_id] = 0;
    }

    if (returned_list_id < ib_free_nodes_counter) {
        uint returned_node_id = ib_returned_nodes_list[returned_list_id];
        ib_push_free_node_id(returned_node_id);

        ib_returned_nodes_list[returned_list_id] = 0;
    }
}
