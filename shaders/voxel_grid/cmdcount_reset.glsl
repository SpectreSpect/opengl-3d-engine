#version 430
layout(local_size_x = 1) in;

struct FrameCounters {uint write_count; uint dirty_count; uint cmd_count; uint free_count; uint failed_dirty_count; };
layout(std430, binding=5) buffer FrameCountersBuf { FrameCounters counters; };
// counters = { voxelWriteCount, dirtyCount, cmdCount, freeCount }

uniform uint u_min_free_pages;

void main() {
    counters.cmd_count = 0u;
}
