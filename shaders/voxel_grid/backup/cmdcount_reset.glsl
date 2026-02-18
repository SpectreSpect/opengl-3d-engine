#version 430
layout(local_size_x = 1) in;

layout(std430, binding=5) buffer FrameCounters { uvec4 counters; };
// counters = { voxelWriteCount, dirtyCount, cmdCount, freeCount }

void main() {
    counters.z = 0u;
}
