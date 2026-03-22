#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <iostream>
#include <string>
#include <chrono>
#include <cmath>

#include "engine3d.h"
#include "opengl_engine.h"
#include "vulkan_engine.h"
#include <array>
#include <tuple>

#include "vao.h"
#include "vbo.h"
#include "ebo.h"
#include "vertex_layout.h"
#include "program.h"
#include "camera.h"
#include "mesh.h"
#include "cube.h"
#include "window.h"
#include "fps_camera_controller.h"
#include "voxel_engine/chunk.h"
#include "voxel_engine/voxel_grid.h"
#include "imgui_layer.h"
#include "voxel_rastorizator.h"
#include "ui_elements/triangle_controller.h"
#include "triangle.h"
#include "a_star/a_star.h"
#include "line.h"
#include "a_star/nonholonomic_a_star.h"
#include "a_star/reeds_shepp.h"

#include <cstdint>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include "point.h"
#include "point_cloud/point_cloud_frame.h"
#include "point_cloud/point_cloud_video.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>
#include <cmath>
#include <cstdint>
#include "math_utils.h"
#include "light_source.h"
#include "circle_cloud.h"
#include "texture.h"
#include "cubemap.h"
#include "skybox.h"
#include "framebuffer.h"
#include "sphere.h"
#include "cube.h"
#include "quad.h"
#include "texture_manager.h"
#include "pbr_skybox.h"
#include <algorithm>
#include <random>
#include "third_person_camera_controller.h"
#include "spider.h"
#include "vulkan_window.h"


struct PipelineObjects {
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

    VkBuffer uniformBuffer = VK_NULL_HANDLE;
    VkDeviceMemory uniformBufferMemory = VK_NULL_HANDLE;
};

struct Vertex {
    glm::vec4 position;
    glm::vec4 normal;
    glm::vec4 color;
};

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};


std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    file.close();

    return buffer;
}


VkShaderModule createShaderModule(VkDevice& device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VulkanEngine::vk_check(
        vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule),
        "vkCreateShaderModule"
    );

    return shaderModule;
}

void createBuffer(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& bufferMemory)
{
    buffer = VK_NULL_HANDLE;
    bufferMemory = VK_NULL_HANDLE;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VulkanEngine::vk_check(
        vkCreateBuffer(device, &bufferInfo, nullptr, &buffer),
        "vkCreateBuffer"
    );

    VkMemoryRequirements memRequirements{};
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VulkanEngine::findMemoryType(
        physicalDevice,
        memRequirements.memoryTypeBits,
        properties
    );

    VulkanEngine::vk_check(
        vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory),
        "vkAllocateMemory"
    );

    VulkanEngine::vk_check(
        vkBindBufferMemory(device, buffer, bufferMemory, 0),
        "vkBindBufferMemory"
    );
}


PipelineObjects createGraphicsPipeline(
    VulkanEngine& engine,
    VkDevice device,
    VkRenderPass renderPass,
    VkExtent2D swapchainExtent,
    VkShaderModule vertModule,
    VkShaderModule fragModule
) {
    PipelineObjects out{};

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertStage, fragStage };

    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(Vertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 3> attrDescs{};

    attrDescs[0].location = 0;
    attrDescs[0].binding = 0;
    attrDescs[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrDescs[0].offset = offsetof(Vertex, position);

    attrDescs[1].location = 1;
    attrDescs[1].binding = 0;
    attrDescs[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrDescs[1].offset = offsetof(Vertex, normal);

    attrDescs[2].location = 2;
    attrDescs[2].binding = 0;
    attrDescs[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrDescs[2].offset = offsetof(Vertex, color);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDescs.size());
    vertexInput.pVertexAttributeDescriptions = attrDescs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchainExtent.width);
    viewport.height = static_cast<float>(swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VulkanEngine::vk_check(
        vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout),
        "vkCreateDescriptorSetLayout"
    );

    VkBuffer uniformBuffer;
    VkDeviceMemory uniformBufferMemory;

    createBuffer(
        engine.device,
        engine.physicalDevice,
        sizeof(UniformBufferObject),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        uniformBuffer,
        uniformBufferMemory
    );

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VulkanEngine::vk_check(
        vkCreateDescriptorPool(engine.device, &poolInfo, nullptr, &descriptorPool),
        "vkCreateDescriptorPool"
    );

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VulkanEngine::vk_check(
        vkAllocateDescriptorSets(engine.device, &allocInfo, &descriptorSet),
        "vkAllocateDescriptorSets"
    );

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = uniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(engine.device, 1, &descriptorWrite, 0, nullptr);


    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    VulkanEngine::vk_check(
        vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &out.layout),
        "vkCreatePipelineLayout"
    );

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = out.layout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VulkanEngine::vk_check(
        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &out.pipeline),
        "vkCreateGraphicsPipelines"
    );


    out.descriptorSetLayout = descriptorSetLayout;
    out.descriptorPool = descriptorPool;
    out.descriptorSet = descriptorSet;
    out.uniformBuffer = uniformBuffer;
    out.uniformBufferMemory = uniformBufferMemory;

    return out;
}

uint32_t findMemoryType(
    VkPhysicalDevice physicalDevice,
    uint32_t typeFilter,
    VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type");
}




float clear_col[4] = {0, 0, 0, 1};

int main() {
    VulkanEngine engine = VulkanEngine();
    VulkanWindow window = VulkanWindow(&engine, 1280, 720, "3D visualization");
    engine.set_vulkan_window(&window);

    std::vector<char> vertCode = readFile("/home/spectre/Projects/test_open_3d/vulkan_test.vert.spv");
    std::vector<char> fragCode = readFile("/home/spectre/Projects/test_open_3d/vulkan_test.frag.spv");

    VkShaderModule vertModule = createShaderModule(engine.device, vertCode);
    VkShaderModule fragModule = createShaderModule(engine.device, fragCode);


    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;

    PipelineObjects graphics_pipeline_objects = createGraphicsPipeline(engine, engine.device, engine.renderPass, engine.swapchainExtent, vertModule, fragModule);


    std::vector<Vertex> vertices = {
        // +Z (front)
        { glm::vec4(-0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) },
        { glm::vec4( 0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) },

        // -Z (back)
        { glm::vec4( 0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f) },
        { glm::vec4(-0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f) },

        // +X (right)
        { glm::vec4( 0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f) },
        { glm::vec4( 0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f) },

        // -X (left)
        { glm::vec4(-0.5f, -0.5f, -0.5f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 0.0f), glm::vec4(1.0f, 1.0f, 0.0f, 1.0f) },
        { glm::vec4(-0.5f, -0.5f,  0.5f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 0.0f), glm::vec4(1.0f, 1.0f, 0.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f,  0.5f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 0.0f), glm::vec4(1.0f, 1.0f, 0.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f, -0.5f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 0.0f), glm::vec4(1.0f, 1.0f, 0.0f, 1.0f) },

        // +Y (top)
        { glm::vec4(-0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  1.0f,  0.0f, 0.0f), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  1.0f,  0.0f, 0.0f), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  1.0f,  0.0f, 0.0f), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  1.0f,  0.0f, 0.0f), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f) },

        // -Y (bottom)
        { glm::vec4(-0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 0.0f, -1.0f,  0.0f, 0.0f), glm::vec4(0.0f, 1.0f, 1.0f, 1.0f) },
        { glm::vec4( 0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 0.0f, -1.0f,  0.0f, 0.0f), glm::vec4(0.0f, 1.0f, 1.0f, 1.0f) },
        { glm::vec4( 0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 0.0f, -1.0f,  0.0f, 0.0f), glm::vec4(0.0f, 1.0f, 1.0f, 1.0f) },
        { glm::vec4(-0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 0.0f, -1.0f,  0.0f, 0.0f), glm::vec4(0.0f, 1.0f, 1.0f, 1.0f) },
    };

    std::vector<uint32_t> indices = {
        0, 1, 2,  2, 3, 0,        // +Z
        4, 5, 6,  6, 7, 4,        // -Z
        8, 9,10, 10,11, 8,        // +X
        12,13,14, 14,15,12,        // -X
        16,17,18, 18,19,16,        // +Y
        20,21,22, 22,23,20         // -Y
    };


    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    createBuffer(
        engine.device,
        engine.physicalDevice,
        sizeof(Vertex) * vertices.size(),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        vertexBuffer,
        vertexBufferMemory
    );
    void* data = nullptr;
    VulkanEngine::vk_check(
        vkMapMemory(
            engine.device,
            vertexBufferMemory,
            0,
            sizeof(Vertex) * vertices.size(),
            0,
            &data
        ),
        "vkMapMemory(vertex)"
    );
    std::memcpy(data, vertices.data(), sizeof(Vertex) * vertices.size());
    vkUnmapMemory(engine.device, vertexBufferMemory);


    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    createBuffer(
        engine.device,
        engine.physicalDevice,
        sizeof(uint32_t) * indices.size(),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        indexBuffer,
        indexBufferMemory
    );
    void* indexData = nullptr;
    VulkanEngine::vk_check(
        vkMapMemory(
            engine.device,
            indexBufferMemory,
            0,
            sizeof(uint32_t) * indices.size(),
            0,
            &indexData
        ),
        "vkMapMemory(index)"
    );
    std::memcpy(indexData, indices.data(), sizeof(uint32_t) * indices.size());
    vkUnmapMemory(engine.device, indexBufferMemory);


    



    // Now we need to create the pipline as far as I understand. We will do it here, maybe using some functions that we will also define in this file (main.cpp)

    while(window.is_open()) {
        engine.begin_frame(glm::vec4(0.01f, 0.01f, 0.01f, 1.0f));


        UniformBufferObject ubo{};
        float t = static_cast<float>(glfwGetTime());

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(std::sin(t), 0.0f, 0.0f));
        model = glm::rotate(model, t, glm::vec3(0.0f, 1.0f, 0.0f));

        ubo.model = model;
        
        ubo.view = glm::lookAt(
                        glm::vec3(2.0f, 2.0f, 2.0f),
                        glm::vec3(0.0f, 0.0f, 0.0f),
                        glm::vec3(0.0f, 1.0f, 0.0f)
                    );
        ubo.proj  = glm::perspective(glm::radians(45.0f),
                                    float(engine.swapchainExtent.width) / float(engine.swapchainExtent.height),
                                    0.1f, 10.0f);
        ubo.proj[1][1] *= -1.0f;

        void* mapped = nullptr;
        VulkanEngine::vk_check(
            vkMapMemory(engine.device, graphics_pipeline_objects.uniformBufferMemory, 0, sizeof(ubo), 0, &mapped),
            "vkMapMemory(ubo)"
        );
        std::memcpy(mapped, &ubo, sizeof(ubo));
        vkUnmapMemory(engine.device, graphics_pipeline_objects.uniformBufferMemory);


        vkCmdBindPipeline(
            engine.currentCommandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            graphics_pipeline_objects.pipeline
        );

        vkCmdBindDescriptorSets(engine.currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                                graphics_pipeline_objects.layout, 0, 1, &graphics_pipeline_objects.descriptorSet, 0, nullptr);

        VkBuffer vertexBuffers[] = { vertexBuffer };
        VkDeviceSize offsets[] = { 0 };

        vkCmdBindVertexBuffers(engine.currentCommandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(engine.currentCommandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(engine.currentCommandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

        engine.end_frame();
        engine.poll_events();
    }
}
