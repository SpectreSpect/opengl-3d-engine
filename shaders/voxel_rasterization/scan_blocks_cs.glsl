#version 430
layout(local_size_x = 256) in;

layout(std430, binding=0) readonly buffer In      { uint inData[]; };
layout(std430, binding=1) buffer Out             { uint outData[]; };
layout(std430, binding=2) buffer BlockSums       { uint blockSums[]; };

uniform uint uN;

shared uint temp[256];

void main(){
    uint lid = gl_LocalInvocationID.x;
    uint block = gl_WorkGroupID.x;
    uint idx = block * 256u + lid;

    temp[lid] = (idx < uN) ? inData[idx] : 0u;
    barrier();

    // Hillis–Steele inclusive scan
    for (uint off=1u; off<256u; off<<=1u) {
        uint t = (lid >= off) ? temp[lid - off] : 0u;
        barrier();
        temp[lid] += t;
        barrier();
    }

    uint excl = (lid == 0u) ? 0u : temp[lid - 1u];
    if (idx < uN) outData[idx] = excl;

    if (lid == 255u) {
        blockSums[block] = temp[255u];
    }
}
