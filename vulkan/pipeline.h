#pragma once
#include <utility>
#include <vulkan/vulkan.h>
#include "descriptor_set_bundle.h"

class Pipeline {
public:
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    DescriptorSetBundle* descriptor_set_bundle = nullptr;

    Pipeline() = default;
    virtual ~Pipeline() = default;

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    Pipeline(Pipeline&& other) noexcept
        : pipeline_layout(std::exchange(other.pipeline_layout, VK_NULL_HANDLE)),
          pipeline(std::exchange(other.pipeline, VK_NULL_HANDLE)),
          descriptor_set_bundle(std::exchange(other.descriptor_set_bundle, nullptr)) {}

    Pipeline& operator=(Pipeline&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        pipeline_layout = std::exchange(other.pipeline_layout, VK_NULL_HANDLE);
        pipeline = std::exchange(other.pipeline, VK_NULL_HANDLE);
        descriptor_set_bundle = std::exchange(other.descriptor_set_bundle, nullptr);

        return *this;
    }

    virtual VkPipelineBindPoint get_bind_point() const = 0;
};