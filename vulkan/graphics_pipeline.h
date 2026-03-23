#pragma once
#include "pipeline.h"
#include "vulkan_vertex_layout.h"
#include "video_buffer.h"
#include "shader_module.h"
#include "video_buffer.h"

class VulkanEngine;

class GraphicsPipeline : public Pipeline {
public:
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    
    
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

    VideoBuffer uniform_buffer;

    GraphicsPipeline() = default;
    GraphicsPipeline(VulkanEngine& engine, uint32_t uniform_buffer_size, ShaderModule& vert_module, ShaderModule& frag_module);

    void create(VkDevice& device, VkPhysicalDevice& physical_device, VkRenderPass& render_pass, 
                VkExtent2D& swapchain_extent, uint32_t uniform_buffer_size, VkShaderModule& vert_module, VkShaderModule& frag_module);
    void create(VulkanEngine& engine, uint32_t uniform_buffer_size, VkShaderModule& vert_module, VkShaderModule& frag_module);
    void create(VulkanEngine& engine, uint32_t uniform_buffer_size, ShaderModule& vert_module, ShaderModule& frag_module);
};