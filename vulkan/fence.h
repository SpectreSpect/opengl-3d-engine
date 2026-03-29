#pragma once
#include <vulkan/vulkan.h>
#include <stdexcept>

class Fence {
public:
    VkDevice* device = nullptr;
    VkFence fence;

    Fence() = default;
    Fence(VkDevice& device);
    void create(VkDevice& device);
    void reset();
    void wait_for_fence();
};