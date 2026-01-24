#pragma once
#include "voxel_engine/voxel.h"

class Gridable {
public:
    virtual void set_voxels(std::vector<Voxel>& voxels, std::vector<glm::ivec3> positions) = 0;
    virtual void set_voxel(Voxel& voxel, glm::ivec3 position) = 0;
    virtual const Voxel* get_voxel(Voxel& voxel, glm::ivec3 position) = 0;
};