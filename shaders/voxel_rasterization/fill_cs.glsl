#version 430
layout(local_size_x = 256) in;

layout(std430, binding=0) readonly buffer Positions { vec4 pos[]; };
layout(std430, binding=1) readonly buffer Tris      { uvec4 tri[]; };
layout(std430, binding=2) buffer Cursor            { coherent uint cursor[]; };
layout(std430, binding=3) buffer OutTriIds         { uint outTriIds[]; };

uniform mat4  uTransform;
uniform float uVoxelSize;
uniform int   uChunkSize;    // > 0
uniform ivec3 uChunkOrigin;  // ROI origin in CHUNKS
uniform uvec3 uGridDim;      // ROI dim in CHUNKS
uniform uint  uTriCount;
uniform uint  uOutCapacity;

int chunk_of_vox(int v) {
    // floor(v / chunkSize) через float (без % на отрицательных)
    return int(floor(float(v) / float(uChunkSize)));
}

void main(){
    uint tid = gl_GlobalInvocationID.x;
    if (tid >= uTriCount) return;

    uvec4 t = tri[tid];

    vec3 p0 = (uTransform * pos[t.x]).xyz / uVoxelSize;
    vec3 p1 = (uTransform * pos[t.y]).xyz / uVoxelSize;
    vec3 p2 = (uTransform * pos[t.z]).xyz / uVoxelSize;

    vec3 mn = min(p0, min(p1, p2)) - vec3(1e-4);
    vec3 mx = max(p0, max(p1, p2)) + vec3(1e-4);

    ivec3 vmin = ivec3(floor(mn));
    ivec3 vmax = ivec3(floor(mx));
    vmax = max(vmax, vmin);

    ivec3 cmin = ivec3(chunk_of_vox(vmin.x), chunk_of_vox(vmin.y), chunk_of_vox(vmin.z));
    ivec3 cmax = ivec3(chunk_of_vox(vmax.x), chunk_of_vox(vmax.y), chunk_of_vox(vmax.z));

    // ROI bounds in chunk space (inclusive)
    ivec3 roiMin = uChunkOrigin;
    ivec3 roiMax = uChunkOrigin + ivec3(uGridDim) - ivec3(1);

    // clamp to ROI so we never compute indices from negative rel
    cmin = max(cmin, roiMin);
    cmax = min(cmax, roiMax);

    if (any(greaterThan(cmin, cmax))) return;

    for (int cz = cmin.z; cz <= cmax.z; ++cz)
    for (int cy = cmin.y; cy <= cmax.y; ++cy)
    for (int cx = cmin.x; cx <= cmax.x; ++cx) {
        ivec3 rel = ivec3(cx,cy,cz) - roiMin; // guaranteed >= 0
        uint cidx = (uint(rel.z) * uGridDim.y + uint(rel.y)) * uGridDim.x + uint(rel.x);

        uint dst = atomicAdd(cursor[cidx], 1u);
        if (dst < uOutCapacity) outTriIds[dst] = tid;
    }
}
