#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <iostream>
#include <vector>
#include <array>

namespace math_utils {
    static constexpr uint32_t BITS = 21;
    static constexpr uint64_t MASK = (uint64_t(1) << BITS) - 1;
    static constexpr int64_t OFFSET = int64_t(1) << (BITS - 1);

    static glm::vec3 bary_coords(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& p) {
        glm::vec3 v0 = b - a;
        glm::vec3 v1 = c - a;
        glm::vec3 v2 = p - a;

        float d00 = glm::dot(v0, v0);
        float d01 = glm::dot(v0, v1);
        float d11 = glm::dot(v1, v1);
        float d20 = glm::dot(v2, v0);
        float d21 = glm::dot(v2, v1);

        float denom = d00 * d11 - d01 * d01;
        if (std::abs(denom) < 1e-8f) return glm::vec3(-1.0f);

        float v = (d11 * d20 - d01 * d21) / denom;
        float w = (d00 * d21 - d01 * d20) / denom;
        float u = 1.0f - v - w;
        return glm::vec3(u, v, w);
    }

    static int floor_div(int a, int b) {
        int q = a / b;
        int r = a % b;
        if (r < 0) --q;
        return q;
    }

    static int floor_mod(int a, int b) {
        int r = a % b;
        if (r < 0) r += b;
        return r;
    }

    static uint32_t div_up_u32(uint32_t a, uint32_t b) { 
        return (a + b - 1u) / b; 
    }

    static double ms_now() {
        using clock = std::chrono::high_resolution_clock;
        static auto t0 = clock::now();
        auto t = clock::now();
        return std::chrono::duration<double, std::milli>(t - t0).count();
    }

    static uint64_t pack_key(int32_t cx, int32_t cy, int32_t cz) {
        static_assert(BITS > 0 && 3 * BITS <= 64);

        if (cx < -OFFSET || cx > OFFSET - 1 ||
            cy < -OFFSET || cy > OFFSET - 1 ||
            cz < -OFFSET || cz > OFFSET - 1) {
            throw std::out_of_range("chunk coord out of packable range");
        }

        auto enc = [](int32_t c) -> uint64_t {
            uint64_t u = static_cast<uint64_t>(static_cast<int64_t>(c) + OFFSET);
            return u & MASK;
        };

        uint64_t ux = enc(cx);
        uint64_t uy = enc(cy);
        uint64_t uz = enc(cz);

        return (ux << (BITS * 2)) | (uy << BITS) | uz;
    }

    static glm::ivec3 unpack_key(uint64_t key) {
        static_assert(BITS > 0 && 3 * BITS <= 64);

        auto dec = [](uint64_t u) -> int32_t {
            // u в диапазоне [0, 2^BITS - 1]
            return static_cast<int32_t>(static_cast<int64_t>(u) - OFFSET);
        };

        uint64_t ux = (key >> (BITS * 2)) & MASK;
        uint64_t uy = (key >> BITS)       & MASK;
        uint64_t uz =  key                & MASK;

        return { dec(ux), dec(uy), dec(uz) };
    }

    static uint32_t next_pow2_u32(uint32_t x) {
        if (x <= 1) return 1;
        x--;
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        return x + 1;
    }

    static glm::uvec2 split_u64(uint64_t k) {
        return glm::uvec2(uint32_t(k & 0xFFFFFFFFull), uint32_t(k >> 32));
    }

    static void normalize_plane(glm::vec4& p) {
        float len = glm::length(glm::vec3(p));
        if (len > 0.0f) p /= len;
    }

    static std::array<glm::vec4, 6> extract_frustum_planes(const glm::mat4& viewProj) {
        glm::mat4 t = glm::transpose(viewProj);

        glm::vec4 row0 = t[0];
        glm::vec4 row1 = t[1];
        glm::vec4 row2 = t[2];
        glm::vec4 row3 = t[3];

        std::array<glm::vec4, 6> p;
        p[0] = row3 + row0; // left
        p[1] = row3 - row0; // right
        p[2] = row3 + row1; // bottom
        p[3] = row3 - row1; // top
        p[4] = row3 + row2; // near
        p[5] = row3 - row2; // far

        for (auto& pl : p) normalize_plane(pl);
        return p;
    }

}