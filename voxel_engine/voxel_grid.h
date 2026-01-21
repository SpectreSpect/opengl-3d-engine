#pragma once
#include "chunk.h"
#include <unordered_map>
#include <iostream>
#include <unistd.h>


constexpr int BITS = 21;
constexpr uint64_t MASK = (1ull << BITS) - 1; // 0x1FFFFF
constexpr int OFFSET = (1 << (BITS-1)); // offset to encode signed -> unsigned

class VoxelGrid : public Drawable, public Transformable {
public:
    int chunk_render_distance = 8;
    glm::ivec3 chunk_size;
    std::unordered_map<uint64_t, Chunk*> chunks;
    Chunk* test_chunk;

    VoxelGrid(glm::ivec3 chunk_size, int chunk_render_distance = 8);

    uint64_t pack_key(int32_t cx, int32_t cy, int32_t cz) {
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


    void unpack_key(uint64_t key, int32_t &cx, int32_t &cy, int32_t &cz) {
        uint64_t ux = (key >> (BITS*2)) & MASK;
        uint64_t uy = (key >> BITS) & MASK;
        uint64_t uz = key & MASK;
        cx = (int)( (int)ux - OFFSET );
        cy = (int)( (int)uy - OFFSET );
        cz = (int)( (int)uz - OFFSET );
    }

    bool is_voxel_free(glm::ivec3 pos);

    void draw(RenderState state) override;
};