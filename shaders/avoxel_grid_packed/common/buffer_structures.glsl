#pragma once

struct FrameCounters {
    uint write_count; 
    uint dirty_count;
    uint cmd_count;
    uint free_count;
    uint failed_dirty_count;
    uint count_vb_free_pages;
    uint count_ib_free_pages;
};

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