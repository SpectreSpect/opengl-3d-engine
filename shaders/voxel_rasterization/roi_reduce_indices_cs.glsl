#version 430
layout(local_size_x = 256) in;

// VBO как сырой float-массив (как ты уже сделал)
layout(std430, binding=0) readonly buffer Vertices { float vtx[]; };
// EBO как uint[] индексы
layout(std430, binding=1) readonly buffer Indices  { uint  idx[]; };

// out: vec4 pairs: outPairs[2*group+0] = (min.xyz, _)
//                   outPairs[2*group+1] = (max.xyz, _)
layout(std430, binding=2) buffer OutPairs { vec4 outPairs[]; };

uniform mat4  uTransform;
uniform float uVoxelSize;

// кол-во ИНДЕКСОВ (не треугольников!)
uniform uint  uIndexCount;

uniform uint  uStrideF;   // stride в float
uniform uint  uPosOffF;   // offset позиции в float (внутри вершины)

// shared редукция
shared vec3 smin[256];
shared vec3 smax[256];

bool finite3(vec3 v){
    return !any(isnan(v)) && !any(isinf(v));
}

void main(){
    uint lid   = gl_LocalInvocationID.x;
    uint group = gl_WorkGroupID.x;
    uint gid   = group * 256u + lid;

    // init
    vec3 mn = vec3( 1.0/0.0);
    vec3 mx = vec3(-1.0/0.0);

    if (gid < uIndexCount) {
        uint vid = idx[gid];

        uint base = vid * uStrideF + uPosOffF;
        vec4 p = vec4(vtx[base + 0u], vtx[base + 1u], vtx[base + 2u], 1.0);

        vec3 pv = (uTransform * p).xyz / uVoxelSize;

        if (finite3(pv)) {
            mn = pv;
            mx = pv;
        }
    }

    smin[lid] = mn;
    smax[lid] = mx;
    barrier();

    // parallel reduce 256 -> 1
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
