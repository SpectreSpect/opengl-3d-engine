#version 430
layout(local_size_x=256) in;

layout(std430, binding=0) buffer OutVoxels { uint outVoxels[]; };

uniform uint uChunkVoxelCount;
uniform uint uActiveCount;

void main(){
    uint voxel = gl_GlobalInvocationID.x;
    uint a = gl_WorkGroupID.y;
    if (voxel >= uChunkVoxelCount) return;
    if (a >= uActiveCount) return;

    outVoxels[a * uChunkVoxelCount + voxel] = 0u;
}
