#include "occupancy_grid_3d.h"



void OccupancyGrid3D::set_cell(glm::ivec3 pos, OccupancyCell occupancy_cell) {
    uint64_t key = pack_key(pos.x, pos.y, pos.z);
    occupancy_cells[key] = occupancy_cell;
}

OccupancyCell OccupancyGrid3D::get_cell(glm::ivec3 pos) {
    uint64_t key = pack_key(pos.x, pos.y, pos.z);
    auto it = occupancy_cells.find(key);
    auto extra_it = extra_cells.find(key);

    if (extra_it != extra_cells.end())
        return extra_it->second;

    if (it == occupancy_cells.end()) {
        OccupancyCell air = OccupancyCell();
        air.solid = false;
        return air;
    }
    
    return it->second;
}