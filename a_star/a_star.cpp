#include "a_star.h"


AStar::AStar() {
    this->grid = new OccupancyGrid3D();
}

AStar::AStar(VoxelGrid* voxel_grid) {
    this->grid = new VoxelOccupancyGrid3D(voxel_grid);
}