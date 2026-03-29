#pragma once
#include <vulkan/vulkan.h>
#include "pipeline.h"
#include "fence.h"

class CommandPool;
class VideoBuffer;

class CommandBuffer {
public:
    struct SubmitDesc {
        VkQueue queue = VK_NULL_HANDLE;

        Fence* fence = nullptr;

        uint32_t wait_semaphore_count = 0;
        const VkSemaphore* wait_semaphores = nullptr;
        const VkPipelineStageFlags* wait_dst_stage_mask = nullptr;

        uint32_t signal_semaphore_count = 0;
        const VkSemaphore* signal_semaphores = nullptr;
    };

    VkDevice* device = nullptr;
    CommandPool* command_pool = nullptr;
    VkCommandBuffer buffer = VK_NULL_HANDLE;

    CommandBuffer() = default;
    CommandBuffer(CommandPool& command_pool);

    void create(CommandPool& command_pool);
    void begin();
    void end();

    void bind_pipeline(Pipeline& pipeline);
    void dispatch(uint32_t x_groups, uint32_t y_groups, uint32_t z_groups);

    void buffer_barrier(
        VideoBuffer& video_buffer,
        VkPipelineStageFlags src_stage,
        VkAccessFlags src_access,
        VkPipelineStageFlags dst_stage,
        VkAccessFlags dst_access
    );

    void fill_buffer(VideoBuffer& video_buffer, uint32_t data);
    void submit(const SubmitDesc& desc);
    void submit_and_wait(VkQueue queue, Fence& fence);
};