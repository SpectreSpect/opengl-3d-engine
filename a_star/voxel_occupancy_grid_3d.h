#include "occupancy_grid_3d.h"
#include "../voxel_engine/voxel_grid.h"

class VoxelOccupancyGrid3D : public OccupancyGrid3D{
public:
    VoxelGrid* voxel_grid;
    VoxelOccupancyGrid3D(VoxelGrid* voxel_grid);

    // void set_cell(glm::ivec3 pos, OccupancyCell* occupancy_cell) override;
    OccupancyCell get_cell(glm::ivec3 pos) override;
};