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