// #version 430
// layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

// layout(std430, binding=0) readonly buffer Positions { vec4 pos[]; };
// layout(std430, binding=1) readonly buffer Tris      { uvec4 tri[]; };
// layout(std430, binding=2) buffer Counters          { coherent uint counters[]; };

// // dbg layout (int[32]):
// // dbg[0]  = triangles processed
// // dbg[1]  = "written once" flag (0->1)
// // dbg[2..4]   vmin xyz
// // dbg[5..7]   vmax xyz
// // dbg[8..10]  cmin xyz
// // dbg[11..13] cmax xyz
// // dbg[14..16] uChunkOrigin xyz
// // dbg[17]     how many times in_roi==true (total adds)
// // dbg[18..20] uGridDim xyz (as int)
// layout(std430, binding=3) buffer Debug { coherent int dbg[]; };

// uniform mat4  uTransform;
// uniform float uVoxelSize;
// uniform int   uChunkSize;
// uniform ivec3 uChunkOrigin;
// uniform uvec3 uGridDim;
// uniform uint  uTriCount;

// int floor_div_pos(int a, int b) {
//     int q = a / b;
//     int r = a % b;
//     if (r < 0) q -= 1; // b > 0
//     return q;
// }

// bool in_roi(ivec3 c, out uint idx) {
//     ivec3 rel = c - uChunkOrigin;
//     if (rel.x < 0 || rel.y < 0 || rel.z < 0) return false;
//     if (rel.x >= int(uGridDim.x) || rel.y >= int(uGridDim.y) || rel.z >= int(uGridDim.z)) return false;

//     idx = (uint(rel.z) * uGridDim.y + uint(rel.y)) * uGridDim.x + uint(rel.x);
//     return true;
// }


// void main(){
//     uint tid = gl_GlobalInvocationID.x;
//     if (tid >= uTriCount) return;

//     atomicAdd(dbg[0], 1);

//     uvec4 t = tri[tid];

//     vec3 p0 = (uTransform * pos[t.x]).xyz / uVoxelSize;
//     vec3 p1 = (uTransform * pos[t.y]).xyz / uVoxelSize;
//     vec3 p2 = (uTransform * pos[t.z]).xyz / uVoxelSize;

//     vec3 mn = min(p0, min(p1, p2));
//     vec3 mx = max(p0, max(p1, p2));

//     mn -= vec3(1e-4);
//     mx += vec3(1e-4);

//     ivec3 vmin = ivec3(floor(mn));
//     ivec3 vmax = ivec3(ceil(mx)) - ivec3(1);
//     vmax = max(vmax, vmin);

//     ivec3 cmin = ivec3(
//         floor_div_pos(vmin.x, uChunkSize),
//         floor_div_pos(vmin.y, uChunkSize),
//         floor_div_pos(vmin.z, uChunkSize)
//     );
//     ivec3 cmax = ivec3(
//         floor_div_pos(vmax.x, uChunkSize),
//         floor_div_pos(vmax.y, uChunkSize),
//         floor_div_pos(vmax.z, uChunkSize)
//     );

//     // записать один раз для первого попавшего tid
//     if (atomicCompSwap(dbg[1], 0, 1) == 0) {
//         dbg[2]=vmin.x; dbg[3]=vmin.y; dbg[4]=vmin.z;
//         dbg[5]=vmax.x; dbg[6]=vmax.y; dbg[7]=vmax.z;
//         dbg[8]=cmin.x; dbg[9]=cmin.y; dbg[10]=cmin.z;
//         dbg[11]=cmax.x; dbg[12]=cmax.y; dbg[13]=cmax.z;
//         dbg[14]=uChunkOrigin.x; dbg[15]=uChunkOrigin.y; dbg[16]=uChunkOrigin.z;
//         dbg[18]=int(uGridDim.x); dbg[19]=int(uGridDim.y); dbg[20]=int(uGridDim.z);
//     }

//     for (int cz=cmin.z; cz<=cmax.z; ++cz)
//     for (int cy=cmin.y; cy<=cmax.y; ++cy)
//     for (int cx=cmin.x; cx<=cmax.x; ++cx){
//         uint cidx;
//         if (!in_roi(ivec3(cx,cy,cz), cidx)) continue;
//         atomicAdd(dbg[17], 1);
//         atomicAdd(counters[cidx], 1u);
//     }
// }


#version 430
layout(local_size_x = 256) in;

layout(std430, binding=0) readonly buffer Positions { vec4 pos[]; };
layout(std430, binding=1) readonly buffer Tris      { uvec4 tri[]; };
layout(std430, binding=2) buffer Counters          { coherent uint counters[]; };

uniform mat4  uTransform;
uniform float uVoxelSize;
uniform int   uChunkSize;   // > 0
uniform ivec3 uChunkOrigin; // ROI origin in CHUNKS
uniform uvec3 uGridDim;     // ROI dim in CHUNKS
uniform uint  uTriCount;

int chunk_of_vox(int v) {
    // floor(v / chunkSize) через float, чтобы не трогать % на отрицательных.
    // При координатах до ~16 млн всё целочисленно точно (float mantissa 24 бита).
    return int(floor(float(v) / float(uChunkSize)));
}

void main() {
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

    // clamp в ROI чтобы вообще убрать in_roi и риск uint-cast на отрицательных
    cmin = max(cmin, roiMin);
    cmax = min(cmax, roiMax);

    if (any(greaterThan(cmin, cmax))) return;

    for (int cz = cmin.z; cz <= cmax.z; ++cz)
    for (int cy = cmin.y; cy <= cmax.y; ++cy)
    for (int cx = cmin.x; cx <= cmax.x; ++cx) {
        ivec3 rel = ivec3(cx,cy,cz) - roiMin; // гарантированно >= 0
        uint idx = (uint(rel.z) * uGridDim.y + uint(rel.y)) * uGridDim.x + uint(rel.x);
        atomicAdd(counters[idx], 1u);
    }
}