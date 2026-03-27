#include "command_buffer.h"
#include "command_pool.h"


CommandBuffer::CommandBuffer(CommandPool& command_pool) {
    create(command_pool);
}

void CommandBuffer::create(CommandPool& command_pool) {
   if (!command_pool.device)
        throw std::runtime_error("Device was nullptr.");
    
    this->command_pool = &command_pool;
    this->device = command_pool.device;
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = command_pool.pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    vulkan_utils::vk_check(vkAllocateCommandBuffers(*command_pool.device, &allocInfo, &buffer),
             "vkAllocateCommandBuffers");
}

void CommandBuffer::begin() {
    if (!device)
        throw std::runtime_error("device was nullptr");

    vulkan_utils::vk_check(
        vkResetCommandBuffer(buffer, 0),
        "vkResetCommandBuffer"
    );
    
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(buffer, &begin_info) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin command buffer");
    }
}

void CommandBuffer::end() {
    if (vkEndCommandBuffer(buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to end command buffer");
    }
}

void CommandBuffer::bind_pipeline(Pipeline& pipeline) {
    vkCmdBindPipeline(buffer, pipeline.get_bind_point(), pipeline.pipeline);
    vkCmdBindDescriptorSets(buffer, pipeline.get_bind_point(), pipeline.pipeline_layout, 0, 1, 
                            &pipeline.descriptor_set_bundle->descriptor_set.descriptor_set, 0, nullptr);
}

void CommandBuffer::dispatch(uint32_t x_groups, uint32_t y_groups, uint32_t z_groups) {
    vkCmdDispatch(buffer, x_groups, y_groups, z_groups);
}

void CommandBuffer::memory_barrier(VideoBuffer& video_buffer) {
    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = video_buffer.buffer;
    barrier.offset = 0;
    barrier.size = video_buffer.size_bytes;

    vkCmdPipelineBarrier(buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, 
                         nullptr, 1, &barrier, 0, nullptr);
}

void CommandBuffer::submit(Fence& fence) {
    if (!command_pool)
        throw std::runtime_error("command pool was null");
    
    if (!device)
        throw std::runtime_error("device was nullptr");

    vulkan_utils::vk_check(
        vkResetFences(*device, 1, &fence.fence),
        "vkResetFences"
    );

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &buffer;

    if (vkQueueSubmit(command_pool->compute_queue, 1, &submitInfo, fence.fence) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit compute work");
    }
}


    // commandBuffers.resize(swapchainImages.size());

    // VkCommandBufferAllocateInfo allocInfo{};
    // allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    // allocInfo.commandPool = commandPool;
    // allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    // allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    // vk_check(vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()),
    //          "vkAllocateCommandBuffers");