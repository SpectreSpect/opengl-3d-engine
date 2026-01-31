#version 430
layout(local_size_x = 256) in;

layout(std430, binding=0) readonly buffer Vertices { float vertex_data[]; };
layout(std430, binding=1) readonly buffer Ind       { uint idx[]; };

layout(std430, binding=2) readonly buffer Offsets   { uint offsets[]; };
layout(std430, binding=3) readonly buffer TriIds    { uint triIds[]; };
layout(std430, binding=4) buffer OutVoxels          { uint outVoxels[]; };
layout(std430, binding=5) readonly buffer ActiveChunks { uint activeChunk[]; };


uniform mat4  uTransform;
uniform float uVoxelSize;
uniform int   uChunkSize;
uniform ivec3 uChunkOrigin;
uniform uvec3 uGridDim;
uniform uint  uChunkVoxelCount;
uniform uint  uChunkCount;
uniform uint uActiveCount;
uniform uint uTriCount;
uniform uint vertex_stride_f;
uniform uint pos_offset_f;


bool axisOverlap(vec3 axis, vec3 v0, vec3 v1, vec3 v2, vec3 halfSize)
{
    // Если ось почти нулевая (дег. ребро/треугольник), тест пропускаем
    float len2 = dot(axis, axis);
    if (len2 < 1e-12) return true;

    float p0 = dot(v0, axis);
    float p1 = dot(v1, axis);
    float p2 = dot(v2, axis);

    float mn = min(p0, min(p1, p2));
    float mx = max(p0, max(p1, p2));

    float r = dot(abs(axis), halfSize);
    return !(mn > r || mx < -r);
}

bool planeBoxOverlap(vec3 normal, vec3 v0, vec3 halfSize)
{
    // Проверка пересечения плоскости треугольника с AABB
    vec3 vmin, vmax;

    // Для каждой координаты выбираем экстремальные точки бокса относительно normal
    vmin.x = (normal.x > 0.0) ? -halfSize.x - v0.x :  halfSize.x - v0.x;
    vmax.x = (normal.x > 0.0) ?  halfSize.x - v0.x : -halfSize.x - v0.x;

    vmin.y = (normal.y > 0.0) ? -halfSize.y - v0.y :  halfSize.y - v0.y;
    vmax.y = (normal.y > 0.0) ?  halfSize.y - v0.y : -halfSize.y - v0.y;

    vmin.z = (normal.z > 0.0) ? -halfSize.z - v0.z :  halfSize.z - v0.z;
    vmax.z = (normal.z > 0.0) ?  halfSize.z - v0.z : -halfSize.z - v0.z;

    if (dot(normal, vmin) > 0.0) return false;
    return dot(normal, vmax) >= 0.0;
}

bool triBoxOverlap(vec3 boxCenter, vec3 halfSize, vec3 p0, vec3 p1, vec3 p2)
{
    // Переводим в координаты бокса (центр бокса в 0)
    vec3 v0 = p0 - boxCenter;
    vec3 v1 = p1 - boxCenter;
    vec3 v2 = p2 - boxCenter;

    // 1) AABB тест: tri AABB vs box
    vec3 mn = min(v0, min(v1, v2));
    vec3 mx = max(v0, max(v1, v2));
    if (mn.x >  halfSize.x || mx.x < -halfSize.x) return false;
    if (mn.y >  halfSize.y || mx.y < -halfSize.y) return false;
    if (mn.z >  halfSize.z || mx.z < -halfSize.z) return false;

    // Рёбра треугольника
    vec3 e0 = v1 - v0;
    vec3 e1 = v2 - v1;
    vec3 e2 = v0 - v2;

    // 2) 9 SAT тестов: (edge x axisX/Y/Z)
    // axis = edge x X => (0, edge.z, -edge.y) (знак неважен)
    if (!axisOverlap(vec3(0.0,  e0.z, -e0.y), v0, v1, v2, halfSize)) return false;
    if (!axisOverlap(vec3(0.0,  e1.z, -e1.y), v0, v1, v2, halfSize)) return false;
    if (!axisOverlap(vec3(0.0,  e2.z, -e2.y), v0, v1, v2, halfSize)) return false;

    // axis = edge x Y => (-edge.z, 0, edge.x)
    if (!axisOverlap(vec3(-e0.z, 0.0,  e0.x), v0, v1, v2, halfSize)) return false;
    if (!axisOverlap(vec3(-e1.z, 0.0,  e1.x), v0, v1, v2, halfSize)) return false;
    if (!axisOverlap(vec3(-e2.z, 0.0,  e2.x), v0, v1, v2, halfSize)) return false;

    // axis = edge x Z => (edge.y, -edge.x, 0)
    if (!axisOverlap(vec3( e0.y, -e0.x, 0.0), v0, v1, v2, halfSize)) return false;
    if (!axisOverlap(vec3( e1.y, -e1.x, 0.0), v0, v1, v2, halfSize)) return false;
    if (!axisOverlap(vec3( e2.y, -e2.x, 0.0), v0, v1, v2, halfSize)) return false;

    // 3) plane-box
    vec3 normal = cross(e0, v2 - v0);
    if (!planeBoxOverlap(normal, v0, halfSize)) return false;

    return true;
}


void main() {
    uint voxelIndex = gl_GlobalInvocationID.x;
    if (voxelIndex >= uChunkVoxelCount) return;

    uint activeIdx = gl_WorkGroupID.y;
    if (activeIdx >= uActiveCount) return;

    uint chunkIndex = activeChunk[activeIdx]; // индекс в dense ROI

    uint xy = uGridDim.x * uGridDim.y;
    uint cz = chunkIndex / xy;
    uint rem = chunkIndex - cz * xy;
    uint cy = rem / uGridDim.x;
    uint cx = rem - cy * uGridDim.x;


    uint cs = uint(uChunkSize);

    // chunkCoord in chunk-space
    ivec3 chunkCoord = uChunkOrigin + ivec3(int(cx), int(cy), int(cz));
    ivec3 chunkOriginVox = chunkCoord * uChunkSize;

    // decode voxelIndex -> local (x,y,z)
    uint z = voxelIndex / (cs*cs);
    uint rem2 = voxelIndex - z*(cs*cs);
    uint y = rem2 / cs;
    uint x = rem2 - y*cs;

    vec3 boxcenter = vec3(float(x)+0.5, float(y)+0.5, float(z)+0.5);
    vec3 halfsize  = vec3(0.5001);

    uint begin = offsets[chunkIndex];
    uint end   = offsets[chunkIndex + 1u];

    uint outVal = 0u;

    for (uint it = begin; it < end; ++it) {
        uint tid = triIds[it];
        uint i0 = idx[tid*3u + 0u];
        uint i1 = idx[tid*3u + 1u];
        uint i2 = idx[tid*3u + 2u];

        uint v0_base = i0 * vertex_stride_f + pos_offset_f;
        uint v1_base = i1 * vertex_stride_f + pos_offset_f;
        uint v2_base = i2 * vertex_stride_f + pos_offset_f;

        vec4 v0 = vec4(vertex_data[v0_base + 0], vertex_data[v0_base + 1], vertex_data[v0_base + 2], 1.0f);
        vec4 v1 = vec4(vertex_data[v1_base + 0], vertex_data[v1_base + 1], vertex_data[v1_base + 2], 1.0f);
        vec4 v2 = vec4(vertex_data[v2_base + 0], vertex_data[v2_base + 1], vertex_data[v2_base + 2], 1.0f);

        vec3 p0 = (uTransform * v0).xyz / uVoxelSize - vec3(chunkOriginVox);
        vec3 p1 = (uTransform * v1).xyz / uVoxelSize - vec3(chunkOriginVox);
        vec3 p2 = (uTransform * v2).xyz / uVoxelSize - vec3(chunkOriginVox);

        if (!triBoxOverlap(boxcenter, halfsize, p0, p1, p2)) continue;

        outVal = 1u;   // или packRGBA8(...) если хочешь цвет
        break;
    }

    uint outIndex = activeIdx * uChunkVoxelCount + voxelIndex;
    outVoxels[outIndex] = outVal;
}
