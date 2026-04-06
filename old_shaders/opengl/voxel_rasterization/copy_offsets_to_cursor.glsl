#version 430
layout(local_size_x = 256) in;

layout(std430, binding=0) readonly buffer Offsets { uint offsets[]; };
layout(std430, binding=1) buffer Cursor          { uint cursor[]; };

uniform uint uN;

void main(){
    uint i = gl_GlobalInvocationID.x;
    if (i >= uN) return;
    cursor[i] = offsets[i];
}
