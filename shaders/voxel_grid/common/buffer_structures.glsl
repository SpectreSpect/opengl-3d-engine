#pragma once

struct ChunkMeta { 
    uint used; 
    uint key_lo; 
    uint key_hi; 
    uint dirty_flags; 
};

struct ChunkMeshAlloc {
    uint v_startPage;
    uint v_order;
    uint needV;
    uint i_startPage;
    uint i_order;
    uint needI;
    uint need_rebuild;
};

struct Node {
    uint page;
    uint next;
};

#define TYPE_SHIFT 16u
#define VIS_SHIFT  8u
#define TYPE_MASK  0xFFu

struct VoxelData {
    uint type_vis_flags;
    uint color;
};

struct VoxelWrite {
    ivec4    world_voxel; // xyz use, w unused
    VoxelData voxel_data;
    uint     _pad0;
    uint     _pad1;
};

struct Vertex {
    vec4 pos;
    uint color;
    uint face;
    uint pad0;
    uint pad1;
};

struct DrawElementsIndirectCommand {
    uint count;
    uint instanceCount;
    uint firstIndex;
    int  baseVertex;
    uint baseInstance;
};