#include "command_buffer.h"
#include "command_pool.h"
#include "video_buffer.h"
#include "vulkan_utils.h"

CommandBuffer::CommandBuffer(CommandPool& command_pool) {
    create(command_pool);
}

void CommandBuffer::create(CommandPool& command_pool) {
    if (!command_pool.device)
        throw std::runtime_error("device was nullptr");

    this->command_pool = &command_pool;
    this->device = command_pool.device;

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = command_pool.pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    vulkan_utils::vk_check(
        vkAllocateCommandBuffers(*command_pool.device, &allocInfo, &buffer),
        "vkAllocateCommandBuffers"
    );
}

void CommandBuffer::begin() {
    if (!device)
        throw std::runtime_error("device was nullptr");

    vulkan_utils::vk_check(vkResetCommandBuffer(buffer, 0), "vkResetCommandBuffer");

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    vulkan_utils::vk_check(vkBeginCommandBuffer(buffer, &begin_info), "vkBeginCommandBuffer");
}

void CommandBuffer::end() {
    vulkan_utils::vk_check(vkEndCommandBuffer(buffer), "vkEndCommandBuffer");
}

void CommandBuffer::bind_pipeline(Pipeline& pipeline) {
    vkCmdBindPipeline(buffer, pipeline.get_bind_point(), pipeline.pipeline);
    vkCmdBindDescriptorSets(
        buffer,
        pipeline.get_bind_point(),
        pipeline.pipeline_layout,
        0,
        1,
        &pipeline.descriptor_set_bundle->descriptor_set.descriptor_set,
        0,
        nullptr
    );
}

void CommandBuffer::dispatch(uint32_t x_groups, uint32_t y_groups, uint32_t z_groups) {
    vkCmdDispatch(buffer, x_groups, y_groups, z_groups);
}

void CommandBuffer::fill_buffer(VideoBuffer& video_buffer, uint32_t data) {
    vkCmdFillBuffer(buffer, video_buffer.buffer, 0, video_buffer.size_bytes, data);
}

void CommandBuffer::buffer_barrier(
    VideoBuffer& video_buffer,
    VkPipelineStageFlags src_stage,
    VkAccessFlags src_access,
    VkPipelineStageFlags dst_stage,
    VkAccessFlags dst_access
) {
    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = src_access;
    barrier.dstAccessMask = dst_access;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = video_buffer.buffer;
    barrier.offset = 0;
    barrier.size = video_buffer.size_bytes;

    vkCmdPipelineBarrier(
        buffer,
        src_stage,
        dst_stage,
        0,
        0, nullptr,
        1, &barrier,
        0, nullptr
    );
}

void CommandBuffer::submit(const SubmitDesc& desc) {
    if (!device)
        throw std::runtime_error("device was nullptr");

    if (desc.queue == VK_NULL_HANDLE)
        throw std::runtime_error("submit queue was VK_NULL_HANDLE");

    if (desc.fence) {
        desc.fence->reset();
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = desc.wait_semaphore_count;
    submitInfo.pWaitSemaphores = desc.wait_semaphores;
    submitInfo.pWaitDstStageMask = desc.wait_dst_stage_mask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &buffer;
    submitInfo.signalSemaphoreCount = desc.signal_semaphore_count;
    submitInfo.pSignalSemaphores = desc.signal_semaphores;

    vulkan_utils::vk_check(
        vkQueueSubmit(desc.queue, 1, &submitInfo, desc.fence ? desc.fence->fence : VK_NULL_HANDLE),
        "vkQueueSubmit"
    );
}

void CommandBuffer::submit_and_wait(VkQueue queue, Fence& fence) {
    SubmitDesc desc{};
    desc.queue = queue;
    desc.fence = &fence;
    submit(desc);
    fence.wait_for_fence();
}