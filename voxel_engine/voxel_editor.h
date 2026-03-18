#pragma once
// #include "chunk.h"
#include <unordered_map>
#include <iostream>
#include <thread>
#include <set>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <atomic>
#include <unordered_set>
#include <utility>
#include "voxel.h"

class VoxelGrid;
class Chunk;

class VoxelEditor{
public:
    VoxelGrid* voxel_grid;
    //                 chunk_key                    voxel_key
    std::unordered_map<uint64_t, std::unordered_map<uint32_t, Voxel>> edited_voxels;

    VoxelEditor(VoxelGrid* voxel_grid) {
        this->voxel_grid = voxel_grid;
    }
    
    void update_and_schedule();

    void edit_chunk(glm::ivec3 chunk_pos);

    void set(glm::ivec3 pos, const Voxel& voxel);
    Voxel get(glm::ivec3 pos) const;

    static glm::ivec3 unpack_local_id(uint32_t id, glm::ivec3 chunk_size){
        uint32_t sx = (uint32_t)chunk_size.x;
        uint32_t sy = (uint32_t)chunk_size.y;

        uint32_t lx = id % sx;
        uint32_t t  = id / sx;
        uint32_t ly = t % sy;
        uint32_t lz = t / sy;

        return glm::ivec3((int)lx, (int)ly, (int)lz);
    }

    static uint32_t pack_local_id(glm::ivec3 lpos, glm::ivec3 chunk_size){
        return (uint32_t)lpos.x
            + (uint32_t)chunk_size.x * ((uint32_t)lpos.y + (uint32_t)chunk_size.y * (uint32_t)lpos.z);
    }

};