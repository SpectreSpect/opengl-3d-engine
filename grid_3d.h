#pragma once
#include <unordered_map>
#include <cstdint>
#include <stdexcept>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>

constexpr int BITS = 21;
constexpr uint64_t MASK = (1ull << BITS) - 1; // 0x1FFFFF
constexpr int OFFSET = (1 << (BITS-1)); // offset to encode signed -> unsigned

class Grid3D {
public:
    static glm::ivec3 floor_pos(const glm::vec3& p);
    static std::vector<glm::ivec3> line_intersects(glm::vec3 pos1, glm::vec3 pos2);    
    virtual bool is_solid(glm::ivec3 pos) = 0;
    bool adjust_to_ground(std::vector<glm::vec3>& output, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1);
    bool adjust_to_ground(std::vector<glm::ivec3>& output, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1);
    bool adjust_to_ground(glm::vec3& output, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1);
    bool get_closest_invisible_top_pos(glm::ivec3 pos, glm::ivec3 &result, int scan_height);
    bool get_closest_visible_bottom_pos(glm::ivec3 pos, glm::ivec3 &result, int max_drop);
    bool get_ground_positions(glm::vec3 pos1, glm::vec3 pos2, std::vector<glm::ivec3>& output, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1);
    bool get_ground_positions(std::vector<glm::vec3> polyline, std::vector<glm::ivec3>& output, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1);
    // glm::ivec2 floor_pos2(const glm::vec2& p);
    // static uint64_t pack_key2(int32_t x, int32_t y);
    std::vector<glm::ivec3> line_intersects_xz(glm::vec3 pos1, glm::vec3 pos2);

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

    static glm::vec3 unpack_key(uint64_t key) {
        uint64_t ux = (key >> (BITS*2)) & MASK;
        uint64_t uy = (key >> BITS) & MASK;
        uint64_t uz = key & MASK;
        int32_t cx = (int)( (int)ux - OFFSET );
        int32_t cy = (int)( (int)uy - OFFSET );
        int32_t cz = (int)( (int)uz - OFFSET );

        return glm::ivec3(cx, cy, cz);
    }
};