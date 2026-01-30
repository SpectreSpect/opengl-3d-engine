#version 430
layout(local_size_x=256) in;

layout(std430, binding=5) buffer OutVoxels { uint outVoxels[]; };

uniform uint uChunkVoxelCount;
uniform uint uActiveCount;

void main(){
    uint voxel = gl_GlobalInvocationID.x;
    if (voxel >= uChunkVoxelCount) return;

    uint a = gl_WorkGroupID.y;
    if (a >= uActiveCount) return;

    uint outIndex = a * uChunkVoxelCount + voxel;
    outVoxels[outIndex] = 0u;
}
