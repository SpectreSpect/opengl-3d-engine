#include "fence.h"

Fence::Fence(VkDevice& device, bool signaled) {
    create(device, signaled);
}

void Fence::create(VkDevice& device, bool signaled) {
    destroy();

    this->device = &device;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

    if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        throw std::runtime_error("failed to create fence");
    }
}

void Fence::destroy() {
    if (device && fence != VK_NULL_HANDLE) {
        vkDestroyFence(*device, fence, nullptr);
        fence = VK_NULL_HANDLE;
    }
    device = nullptr;
}

void Fence::reset() {
    if (!device)
        throw std::runtime_error("device was null");

    if (vkResetFences(*device, 1, &fence) != VK_SUCCESS) {
        throw std::runtime_error("failed to reset fence");
    }
}

void Fence::wait_for_fence() {
    if (!device)
        throw std::runtime_error("device was null");

    if (vkWaitForFences(*device, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        throw std::runtime_error("failed to wait for fence");
    }
}