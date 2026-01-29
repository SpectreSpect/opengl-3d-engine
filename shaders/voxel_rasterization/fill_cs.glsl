#version 430
layout(local_size_x = 256) in;

layout(std430, binding=0) readonly buffer Positions { vec4 pos[]; };
layout(std430, binding=1) readonly buffer Tris      { uvec4 tri[]; };
layout(std430, binding=2) buffer Cursor            { coherent uint cursor[]; };
layout(std430, binding=3) buffer OutTriIds         { uint outTriIds[]; };

uniform mat4  uTransform;
uniform float uVoxelSize;
uniform int   uChunkSize;
uniform ivec3 uChunkOrigin;
uniform uvec3 uGridDim;
uniform uint  uTriCount;
uniform uint  uOutCapacity;

int floor_div_pos(int a, int b) {
    int q = a / b;
    int r = a % b;
    if (r < 0) q -= 1;
    return q;
}

bool in_roi(ivec3 c, out uint idx) {
    ivec3 rel = c - uChunkOrigin;
    ivec3 dim = ivec3(uGridDim);
    if (any(lessThan(rel, ivec3(0))) || any(greaterThanEqual(rel, dim))) return false;
    idx = (uint(rel.z) * uGridDim.y + uint(rel.y)) * uGridDim.x + uint(rel.x);
    return true;
}

void main(){
    uint tid = gl_GlobalInvocationID.x;
    if (tid >= uTriCount) return;

    uvec4 t = tri[tid];
    vec3 p0 = (uTransform * pos[t.x]).xyz / uVoxelSize;
    vec3 p1 = (uTransform * pos[t.y]).xyz / uVoxelSize;
    vec3 p2 = (uTransform * pos[t.z]).xyz / uVoxelSize;

    vec3 mn = min(p0, min(p1, p2));
    vec3 mx = max(p0, max(p1, p2));

    ivec3 vmin = ivec3(floor(mn));
    ivec3 vmax = ivec3(ceil(mx)) - ivec3(1);
    vmax = max(vmax, vmin);

    ivec3 cmin = ivec3(
        floor_div_pos(vmin.x, uChunkSize),
        floor_div_pos(vmin.y, uChunkSize),
        floor_div_pos(vmin.z, uChunkSize)
    );
    ivec3 cmax = ivec3(
        floor_div_pos(vmax.x, uChunkSize),
        floor_div_pos(vmax.y, uChunkSize),
        floor_div_pos(vmax.z, uChunkSize)
    );

    for (int cz=cmin.z; cz<=cmax.z; ++cz)
    for (int cy=cmin.y; cy<=cmax.y; ++cy)
    for (int cx=cmin.x; cx<=cmax.x; ++cx){
        uint cidx;
        if (!in_roi(ivec3(cx,cy,cz), cidx)) continue;

        uint dst = atomicAdd(cursor[cidx], 1u);
        if (dst < uOutCapacity) outTriIds[dst] = tid;
    }
}
