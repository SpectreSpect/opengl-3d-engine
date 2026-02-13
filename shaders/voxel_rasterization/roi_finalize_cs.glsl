#version 430
layout(local_size_x = 1) in;

layout(std430, binding=0) readonly buffer InFinalPairs { vec4 inPairs[]; };

// пишем ROI сюда (потом можно readback или читать в других compute)
layout(std430, binding=1) buffer RoiOut {
    ivec4 roiOrigin; // xyz
    uvec4 roiDim;    // xyz
};

uniform int  uChunkSize;
uniform int  uPadVoxels;
uniform float uEps; // например 1e-4

bool finite3(vec3 v){
    return !any(isnan(v)) && !any(isinf(v));
}

// floor_div без % (и без int64): делаем через double — тут 1 invocation, цена нулевая
int floor_div_int(int a, int b){
    return int(floor(double(a) / double(b))); // b > 0
}

void main(){
    vec3 mn = inPairs[0].xyz;
    vec3 mx = inPairs[1].xyz;

    // пустой/битый случай
    if (!finite3(mn) || !finite3(mx) || any(greaterThan(mn, mx))) {
        roiOrigin = ivec4(0);
        roiDim    = uvec4(0);
        return;
    }

    mn -= vec3(uEps);
    mx += vec3(uEps);

    ivec3 vmin = ivec3(floor(mn));
    ivec3 vmax = ivec3(floor(mx));
    vmax = max(vmax, vmin);

    if (uPadVoxels > 0) {
        vmin -= ivec3(uPadVoxels);
        vmax += ivec3(uPadVoxels);
    }

    ivec3 cmin = ivec3(
        floor_div_int(vmin.x, uChunkSize),
        floor_div_int(vmin.y, uChunkSize),
        floor_div_int(vmin.z, uChunkSize)
    );
    ivec3 cmax = ivec3(
        floor_div_int(vmax.x, uChunkSize),
        floor_div_int(vmax.y, uChunkSize),
        floor_div_int(vmax.z, uChunkSize)
    );

    ivec3 dim = cmax - cmin + ivec3(1);
    dim = max(dim, ivec3(0));

    roiOrigin = ivec4(cmin, 0);
    roiDim    = uvec4(dim, 0u);
}
