#version 430
layout(local_size_x = 256) in;

layout(std430, binding=0) readonly buffer Vert   { float v[]; };   // VBO as float[]
layout(std430, binding=1) readonly buffer Ind    { uint  idx[]; }; // EBO as uint[]
layout(std430, binding=2) buffer Counters        { coherent uint counters[]; };

uniform mat4  uTransform;
uniform float uVoxelSize;
uniform int   uChunkSize;
uniform ivec3 uChunkOrigin;
uniform uvec3 uGridDim;
uniform uint  uTriCount;

// from CPU (derived from VertexLayout)
uniform uint uStrideF;
uniform uint uPosOffF;

int floor_div_no_mod(int a, int b) {
    int q = a / b;
    int prod = q * b;
    if (prod != a && ((a < 0) != (b < 0)))
        q -= 1;
    return q;
}

vec3 load_pos(uint vid) {
    uint base = vid * uStrideF + uPosOffF;
    return vec3(v[base+0u], v[base+1u], v[base+2u]);
}

bool in_roi(ivec3 c, out uint idxOut) {
    ivec3 rel = c - uChunkOrigin;
    ivec3 dim = ivec3(uGridDim);
    if (any(lessThan(rel, ivec3(0))) || any(greaterThanEqual(rel, dim))) return false;

    // no modulo here
    idxOut = (uint(rel.z) * uGridDim.y + uint(rel.y)) * uGridDim.x + uint(rel.x);
    return true;
}

void main() {
    uint tid = gl_GlobalInvocationID.x;
    if (tid >= uTriCount) return;

    uint i0 = idx[tid*3u + 0u];
    uint i1 = idx[tid*3u + 1u];
    uint i2 = idx[tid*3u + 2u];

    vec3 p0 = (uTransform * vec4(load_pos(i0), 1.0)).xyz / uVoxelSize;
    vec3 p1 = (uTransform * vec4(load_pos(i1), 1.0)).xyz / uVoxelSize;
    vec3 p2 = (uTransform * vec4(load_pos(i2), 1.0)).xyz / uVoxelSize;

    vec3 mn = min(p0, min(p1, p2));
    vec3 mx = max(p0, max(p1, p2));

    mn -= vec3(1e-4);
    mx += vec3(1e-4);

    ivec3 vmin = ivec3(floor(mn));
    ivec3 vmax = ivec3(floor(mx));
    vmax = max(vmax, vmin);

    ivec3 cmin = ivec3(
        floor_div_no_mod(vmin.x, uChunkSize),
        floor_div_no_mod(vmin.y, uChunkSize),
        floor_div_no_mod(vmin.z, uChunkSize)
    );
    ivec3 cmax = ivec3(
        floor_div_no_mod(vmax.x, uChunkSize),
        floor_div_no_mod(vmax.y, uChunkSize),
        floor_div_no_mod(vmax.z, uChunkSize)
    );

    for (int cz=cmin.z; cz<=cmax.z; ++cz)
    for (int cy=cmin.y; cy<=cmax.y; ++cy)
    for (int cx=cmin.x; cx<=cmax.x; ++cx) {
        uint cidx;
        if (!in_roi(ivec3(cx,cy,cz), cidx)) continue;
        atomicAdd(counters[cidx], 1u);
    }
}
