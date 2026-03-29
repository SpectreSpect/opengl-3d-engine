#pragma once
#include <vulkan/vulkan.h>
#include <stdexcept>

class Fence {
public:
    VkDevice* device = nullptr;
    VkFence fence = VK_NULL_HANDLE;

    Fence() = default;
    Fence(VkDevice& device, bool signaled = false);

    void create(VkDevice& device, bool signaled = false);
    void destroy();
    void reset();
    void wait_for_fence();
};