#pragma once
#include "gridable.h"
#include "buffer_object.h"

class IGridableGPU : public IGridable {
public:
    virtual void set_voxels(const BufferObject& voxels_write_data) = 0;
};