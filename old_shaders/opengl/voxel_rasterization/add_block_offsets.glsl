#version 430
layout(local_size_x = 256) in;

layout(std430, binding=0) buffer Out        { uint outData[]; };
layout(std430, binding=1) readonly buffer BlockPrefix { uint prefix[]; };

uniform uint uN;

void main(){
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= uN) return;
    uint block = idx / 256u;
    outData[idx] += prefix[block];
}
