#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "ssbo.h"

class VoxelGridGPU {
public:
    glm::ivec3 chunk_size;
    int count_active_chunks;
    glm::vec3 voxel_size;
    int chunk_hash_table_size;

    struct ChunkMeta {
        bool used;
        int coord_key[3]; // Потом преобразовать в ключ, пока - координаты WARNING!!!
        char dirty_flags;
    };

    struct ChunkMeshMeta {
        uint32_t first_index;
        uint32_t index_count;
        uint32_t base_vertex;
        char mesh_valid;
    };

    struct VoxelData {
        char type;
        char visability;
        uint32_t color; //r8g8b8a8 = 32 bytes
    };

    VoxelGridGPU(glm::ivec3 chunk_size, glm::vec3 voxel_size, uint32_t count_active_chunks, uint32_t chunk_hash_table_size);
    ~VoxelGridGPU();

    void init_active_chunks(glm::ivec3 chunk_size, uint32_t count_active_chunks, const VoxelData& init_voxel_prifab);

    SSBO active_chunks;
    SSBO chunk_meta;
    SSBO chunk_hash_keys;
    SSBO chunk_hash_vals;
    SSBO free_list;
    SSBO active_list;
    SSBO frame_counters;
    SSBO voxel_write_list;
    SSBO equeued;
    SSBO dirty_list;
    SSBO chunk_mesh_meta;
    SSBO global_vertex_buffer;
    SSBO global_index_buffer;
};