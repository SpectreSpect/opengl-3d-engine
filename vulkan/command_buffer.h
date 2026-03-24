#pragma once
#include <vulkan/vulkan.h>
#include "vulkan_utils.h"
#include "pipeline.h"
#include "compute_pipeline.h"
#include "descriptor_set.h"
#include "fence.h"

class CommandPool;

class CommandBuffer {
public:
    VkDevice* device = nullptr;
    CommandPool* command_pool = nullptr;
    VkCommandBuffer buffer;
    CommandBuffer() = default;
    CommandBuffer(CommandPool& command_pool);
    void create(CommandPool& command_pool);
    void begin();
    void end();
    void bind_pipeline(Pipeline& pipeline);
    void dispatch(uint32_t x_groups, uint32_t y_groups, uint32_t z_groups);
    void memory_barrier(VideoBuffer& video_buffer);
    void submit(Fence& fence);
};