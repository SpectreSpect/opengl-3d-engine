#include "voxel_occupancy_grid_3d.h"

VoxelOccupancyGrid3D::VoxelOccupancyGrid3D(VoxelGrid* voxel_grid) {
    this->voxel_grid = voxel_grid;
}

// void VoxelOccupancyGrid3D::set_cell(glm::ivec3 pos, OccupancyCell* occupancy_cell) {

// }

OccupancyCell VoxelOccupancyGrid3D::get_cell(glm::ivec3 pos) {
    Voxel voxel = voxel_grid->get_voxel(pos);

    OccupancyCell occupancy_cell;
    occupancy_cell.solid = voxel.visible;

    return occupancy_cell;
}