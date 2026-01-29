#version 430
layout(local_size_x = 1) in;

layout(std430, binding=0) readonly buffer Counters { uint counters[]; };
layout(std430, binding=1) buffer Offsets          { uint offsets[]; }; // n+1
layout(std430, binding=2) buffer TotalPairs       { uint totalPairs[]; }; // [1]

uniform uint uN;

void main(){
    if (uN == 0u) {
        totalPairs[0] = 0u;
        offsets[0] = 0u;
        return;
    }
    uint last = offsets[uN - 1u] + counters[uN - 1u];
    offsets[uN] = last;
    totalPairs[0] = last;
}
