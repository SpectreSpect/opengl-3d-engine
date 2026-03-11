#pragma once

#define CAT_(A, B) A##B
#define CAT(A, B)  CAT_(A, B)
#define CAT3_(A, B, C) A##B##C
#define CAT3(A, B, C)  CAT3_(A, B, C)
#define PR(PREFIX, NAME) CAT3(PREFIX, _, NAME)

#define INVALID_ID 0xFFFFFFFFu

uint max(uint a, uint b) {
    return a > b ? a : b;
}

uint div_up_u32(uint a, uint b) { 
    return (a + b - 1u) / b; 
}

uint ceil_log2_u32(uint x) {
    if (x <= 1u) return 0u;
    return uint(findMSB(x - 1u) + 1);
}

uint mask_bits(uint bits) { 
    return (bits >= 32u) ? 0xFFFFFFFFu : ((1u << bits) - 1u); 
}

uint hash_uvec2(uvec2 v) {
    uint x = v.x * 1664525u + 1013904223u;
    uint y = v.y * 22695477u + 1u;
    uint h = x ^ (y + (x << 16) + (x >> 16));
    h ^= h >> 16; h *= 0x7feb352du;
    h ^= h >> 15; h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

uvec2 pack_key_uvec2(ivec3 c, uint pack_offset, uint pack_bits) {
    uint B = pack_bits;
    uint m = mask_bits(B);

    uint ux = uint(c.x + pack_offset) & m;
    uint uy = uint(c.y + pack_offset) & m;
    uint uz = uint(c.z + pack_offset) & m;

    uint lo = 0u, hi = 0u;
    lo |= uz;

    // uy at shift B
    if (B < 32u) {
        lo |= (uy << B);
        if (B != 0u && (B + B) > 32u) hi |= (uy >> (32u - B));
    } else {
        hi |= (uy << (B - 32u));
    }

    // ux at shift 2B
    uint s = 2u * B;
    if (s < 32u) {
        lo |= (ux << s);
        if (B != 0u && (s + B) > 32u) hi |= (ux >> (32u - s));
    } else if (s == 32u) {
        hi |= ux;
    } else {
        hi |= (ux << (s - 32u));
    }

    return uvec2(lo, hi);
}