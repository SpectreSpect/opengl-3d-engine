#version 430
layout(local_size_x = 256) in;

layout(std430, binding=0) readonly buffer Counters    { uint counters[]; };
layout(std430, binding=1) buffer ActiveChunks         { uint activeChunks[]; }; // capacity >= uN
layout(std430, binding=2) buffer ActiveCount          { coherent uint activeCount[]; }; // [1]

uniform uint uN;

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= uN) return;

    if (counters[i] == 0u) return;

    uint dst = atomicAdd(activeCount[0], 1u);
    activeChunks[dst] = i;
}
