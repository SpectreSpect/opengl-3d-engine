#include "fence.h"

Fence::Fence(VkDevice& device) {
    create(device);
}

void Fence::create(VkDevice& device) {
    this->device = &device;
    
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        throw std::runtime_error("failed to create fence");
    }
}

void Fence::reset() {
    if (!device)
        throw std::runtime_error("device was null");

    if (vkResetFences(*device, 1, &fence) != VK_SUCCESS) {
        throw std::runtime_error("failed to reset fence");
    }
}

void Fence::wait_for_fence() {
    if (!this->device)
        throw std::runtime_error("device was null");

    vkWaitForFences(*this->device, 1, &fence, VK_TRUE, UINT64_MAX);
}