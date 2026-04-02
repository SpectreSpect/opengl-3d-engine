#include "voxel_point_map.h"


void VoxelPointMap::create(VulkanEngine& engine, uint32_t num_hash_table_slots, uint32_t max_map_point_count) {
    this->engine = &engine;
    this->num_hash_table_slots = num_hash_table_slots;
    this->max_map_point_count = max_map_point_count;

    if (num_hash_table_slots <= 0) {
        std::string message = "VoxelPointMap::create: num_hash_table_slots must be greater than 0";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (num_hash_table_slots <= 0) {
        std::string message = "VoxelPointMap::create: max_map_point_count must be greater than 0";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    map_uniform_buffer.create(engine, sizeof(VoxelPointMapUniform));
    map_hash_table_buffer.create(engine, sizeof(VoxelHashSlotGpu) * num_hash_table_slots);
    map_point_buffer.create(engine, sizeof(PointInstance) * max_map_point_count);
    map_normal_buffer.create(engine, sizeof(glm::vec4) * max_map_point_count);
    map_point_count_buffer.create(engine, sizeof(uint32_t));
}