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
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, Voxel>> edited_voxels;

    VoxelEditor(VoxelGrid* voxel_grid) {
        this->voxel_grid = voxel_grid;
    }
    
    void update_and_schedule();

    void set(glm::ivec3 pos, const Voxel& voxel);
    Voxel get(glm::ivec3 pos) const;
};