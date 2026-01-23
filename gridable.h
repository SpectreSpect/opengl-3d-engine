#include "voxel_engine/voxel.h"

class Gridable {
public:
    virtual void set_voxel(Voxel* voxel) = 0;
    virtual Voxel* get_voxel() = 0;
};