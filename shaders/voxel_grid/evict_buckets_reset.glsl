#version 430
layout(local_size_x = 256) in;

#define INVALID_ID 0xFFFFFFFFu

layout(std430, binding=16) coherent buffer BucketHeads { uint bucket_heads[]; };

uniform uint u_bucket_count;

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < u_bucket_count) bucket_heads[i] = INVALID_ID;
}
