#version 430
layout(local_size_x = 256) in;

layout(std430, binding=0) readonly buffer InPairs  { vec4 inPairs[]; };
layout(std430, binding=1) buffer OutPairs          { vec4 outPairs[]; };

// количество AABB-пар (т.е. groups предыдущего уровня)
uniform uint uPairCount;

shared vec3 smin[256];
shared vec3 smax[256];

bool finite3(vec3 v){
    return !any(isnan(v)) && !any(isinf(v));
}

void main(){
    uint lid   = gl_LocalInvocationID.x;
    uint group = gl_WorkGroupID.x;
    uint i     = group * 256u + lid;

    vec3 mn = vec3( 1.0/0.0);
    vec3 mx = vec3(-1.0/0.0);

    if (i < uPairCount) {
        vec3 a = inPairs[i * 2u + 0u].xyz;
        vec3 b = inPairs[i * 2u + 1u].xyz;

        // если вдруг пара “пустая”
        if (finite3(a)) mn = a;
        if (finite3(b)) mx = b;
    }

    smin[lid] = mn;
    smax[lid] = mx;
    barrier();

    for (uint stride = 128u; stride > 0u; stride >>= 1u) {
        if (lid < stride) {
            smin[lid] = min(smin[lid], smin[lid + stride]);
            smax[lid] = max(smax[lid], smax[lid + stride]);
        }
        barrier();
    }

    if (lid == 0u) {
        outPairs[group * 2u + 0u] = vec4(smin[0], 1.0);
        outPairs[group * 2u + 1u] = vec4(smax[0], 1.0);
    }
}
