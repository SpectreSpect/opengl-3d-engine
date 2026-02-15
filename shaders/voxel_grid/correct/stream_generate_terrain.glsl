#version 430
layout(local_size_x = 256) in;

#define TYPE_SHIFT 16u
#define VIS_SHIFT  8u
#define TYPE_MASK  0xFFu

layout(std430, binding=16) buffer StreamCounters { uvec2 stream; }; // x=loadCount
layout(std430, binding=17) readonly buffer LoadList { uint load_list[]; };

struct VoxelData { uint type_vis_flags; uint color; };
layout(std430, binding=3) buffer ChunkVoxels { VoxelData voxels[]; };

struct ChunkMeta { uint used; uint key_lo; uint key_hi; uint dirty_flags; };
layout(std430, binding=6) buffer ChunkMetaBuf { ChunkMeta meta[]; };

layout(std430, binding=7) buffer EnqueuedBuf { uint enqueued[]; };
layout(std430, binding=8) buffer DirtyListBuf { uint dirty_list[]; };
layout(std430, binding=5) buffer FrameCounters { uvec4 counters; }; // y=dirtyCount

uniform ivec3 u_chunk_dim;
uniform uint  u_voxels_per_chunk;
uniform vec3  u_voxel_size;

uniform uint u_pack_bits;
uniform int  u_pack_offset;

uniform uint u_set_dirty_flag_bits; // 1u
uniform uint u_seed;

// ---- unpack key (без uint64) ----
uint mask_bits(uint bits) { return (bits >= 32u) ? 0xFFFFFFFFu : ((1u << bits) - 1u); }

uint extract_bits_uvec2(uvec2 k, uint shift, uint bits) {
    uint m = mask_bits(bits);
    if (bits == 0u) return 0u;

    if (shift < 32u) {
        uint lo_part = k.x >> shift;
        uint res = lo_part;
        uint rem = 32u - shift;
        if (rem < bits) res |= (k.y << rem);
        return res & m;
    } else {
        uint s = shift - 32u;
        return (k.y >> s) & m;
    }
}

ivec3 unpack_key_to_coord(uvec2 key2) {
    uint B = u_pack_bits;
    uint ux = extract_bits_uvec2(key2, 2u * B, B);
    uint uy = extract_bits_uvec2(key2, 1u * B, B);
    uint uz = extract_bits_uvec2(key2, 0u * B, B);

    return ivec3(int(ux) - u_pack_offset,
                 int(uy) - u_pack_offset,
                 int(uz) - u_pack_offset);
}

// ---- helpers ----
uint voxel_index(ivec3 p) {
    return uint((p.z * u_chunk_dim.y + p.y) * u_chunk_dim.x + p.x);
}

void mark_dirty(uint chunkId) {
    atomicOr(meta[chunkId].dirty_flags, u_set_dirty_flag_bits);
    uint was = atomicExchange(enqueued[chunkId], 1u);
    if (was == 0u) {
        uint di = atomicAdd(counters.y, 1u);
        dirty_list[di] = chunkId;
    }
}

// ---- noise (value noise + fbm) ----
uint hash_u32(uint x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

float hash2(ivec2 p) {
    uint h = uint(p.x) * 374761393u
           + uint(p.y) * 668265263u
           + u_seed     * 2246822519u;

    h = hash_u32(h);
    return float(h) * (1.0 / 4294967295.0);
}

float valueNoise(vec2 x) {
    ivec2 i = ivec2(floor(x));
    vec2  f = fract(x);
    vec2  u = f*f*(3.0 - 2.0*f);

    float a = hash2(i + ivec2(0,0));
    float b = hash2(i + ivec2(1,0));
    float c = hash2(i + ivec2(0,1));
    float d = hash2(i + ivec2(1,1));

    return mix(mix(a,b,u.x), mix(c,d,u.x), u.y);
}

float fbm(vec2 p) {
    float s = 0.0;
    float a = 0.5;
    for (int o=0; o<5; ++o) {
        s += a * valueNoise(p);
        p *= 2.0;
        a *= 0.5;
    }
    return s;
}

uint pack_color(vec3 rgb) {
    rgb = clamp(rgb, 0.0, 1.0);
    uint r = uint(rgb.r * 255.0 + 0.5);
    uint g = uint(rgb.g * 255.0 + 0.5);
    uint b = uint(rgb.b * 255.0 + 0.5);
    return (r) | (g<<8) | (b<<16) | (0xFFu<<24);
}

void main() {
    uint voxelId = gl_GlobalInvocationID.x;
    uint listIdx = gl_GlobalInvocationID.y;

    uint loadCount = stream.x;
    if (listIdx >= loadCount) return;
    if (voxelId >= u_voxels_per_chunk) return;

    uint chunkId = load_list[listIdx];

    ivec3 chunkCoord = unpack_key_to_coord(uvec2(meta[chunkId].key_lo, meta[chunkId].key_hi));

    int lx = int(voxelId % uint(u_chunk_dim.x));
    int ly = int((voxelId / uint(u_chunk_dim.x)) % uint(u_chunk_dim.y));
    int lz = int(voxelId / uint(u_chunk_dim.x * u_chunk_dim.y));
    ivec3 local = ivec3(lx, ly, lz);

    ivec3 worldVoxel = chunkCoord * u_chunk_dim + local;

    // ---- terrain height from fbm(xz) ----
    vec2 xz = vec2(worldVoxel.x, worldVoxel.z) * 0.03; // частота
    float n = fbm(xz); // 0..~1
    float height = 20.0 + n * 30.0; // базовый уровень + амплитуда

    uint type = (float(worldVoxel.y) <= height) ? 1u : 0u;
    uint vis  = (type != 0u) ? 1u : 0u;

    VoxelData vd;
    vd.type_vis_flags = (type << TYPE_SHIFT) | (vis << VIS_SHIFT);
    // цвет: чуть меняем по высоте
    vec3 col = (type != 0u) ? mix(vec3(0.15,0.35,0.10), vec3(0.45,0.30,0.15), n) : vec3(0.0);
    vd.color = pack_color(col);

    voxels[chunkId * u_voxels_per_chunk + voxel_index(local)] = vd;

    // один раз на чанк
    if (voxelId == 0u) {
        mark_dirty(chunkId);
    }
}
