#include "equirect_to_cubemap_pass.h"

#include <array>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>

#include "../../mesh.h"
#include "../vulkan_utils.h"

namespace {

VkCommandBuffer begin_one_time_commands(VulkanEngine& engine) {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = engine.commandPool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vulkan_utils::vk_check(
        vkAllocateCommandBuffers(engine.device, &alloc_info, &cmd),
        "vkAllocateCommandBuffers(EquirectToCubemapPass)"
    );

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vulkan_utils::vk_check(
        vkBeginCommandBuffer(cmd, &begin_info),
        "vkBeginCommandBuffer(EquirectToCubemapPass)"
    );

    return cmd;
}

void end_one_time_commands(VulkanEngine& engine, VkCommandBuffer cmd) {
    vulkan_utils::vk_check(
        vkEndCommandBuffer(cmd),
        "vkEndCommandBuffer(EquirectToCubemapPass)"
    );

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    vulkan_utils::vk_check(
        vkQueueSubmit(engine.graphicsQueue, 1, &submit_info, VK_NULL_HANDLE),
        "vkQueueSubmit(EquirectToCubemapPass)"
    );

    vulkan_utils::vk_check(
        vkQueueWaitIdle(engine.graphicsQueue),
        "vkQueueWaitIdle(EquirectToCubemapPass)"
    );

    vkFreeCommandBuffers(engine.device, engine.commandPool, 1, &cmd);
}

bool has_stencil_component(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
           format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void update_hdr_descriptor(VkDevice device, VkDescriptorSet descriptor_set, const Texture2D& hdr_texture) {
    VkDescriptorImageInfo image_info = hdr_texture.descriptor_info();

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptor_set;
    write.dstBinding = 1;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

void cmd_image_barrier(
    VkCommandBuffer cmd,
    VkImage image,
    VkAccessFlags src_access,
    VkAccessFlags dst_access,
    VkImageLayout old_layout,
    VkImageLayout new_layout,
    VkPipelineStageFlags src_stage,
    VkPipelineStageFlags dst_stage,
    VkImageAspectFlags aspect_mask,
    uint32_t base_mip_level,
    uint32_t level_count,
    uint32_t base_array_layer,
    uint32_t layer_count
) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = src_access;
    barrier.dstAccessMask = dst_access;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect_mask;
    barrier.subresourceRange.baseMipLevel = base_mip_level;
    barrier.subresourceRange.levelCount = level_count;
    barrier.subresourceRange.baseArrayLayer = base_array_layer;
    barrier.subresourceRange.layerCount = layer_count;

    vkCmdPipelineBarrier(
        cmd,
        src_stage,
        dst_stage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

void cmd_generate_cubemap_mips(VkCommandBuffer cmd, const Cubemap& cubemap) {
    if (cubemap.mip_levels <= 1) {
        cmd_image_barrier(
            cmd,
            cubemap.image,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            0, 1,
            0, 6
        );
        return;
    }

    int32_t mip_size = static_cast<int32_t>(cubemap.size);

    for (uint32_t mip = 1; mip < cubemap.mip_levels; ++mip) {
        cmd_image_barrier(
            cmd,
            cubemap.image,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            mip - 1, 1,
            0, 6
        );

        for (uint32_t face = 0; face < 6; ++face) {
            VkImageBlit blit{};
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mip_size, mip_size, 1};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = mip - 1;
            blit.srcSubresource.baseArrayLayer = face;
            blit.srcSubresource.layerCount = 1;

            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {
                std::max(1, mip_size / 2),
                std::max(1, mip_size / 2),
                1
            };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = mip;
            blit.dstSubresource.baseArrayLayer = face;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(
                cmd,
                cubemap.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                cubemap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit,
                VK_FILTER_LINEAR
            );
        }

        cmd_image_barrier(
            cmd,
            cubemap.image,
            VK_ACCESS_TRANSFER_READ_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            mip - 1, 1,
            0, 6
        );

        if (mip_size > 1) {
            mip_size /= 2;
        }
    }

    cmd_image_barrier(
        cmd,
        cubemap.image,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        cubemap.mip_levels - 1, 1,
        0, 6
    );
}

} // namespace


void EquirectToCubemapPass::create(VulkanEngine& engine, uint32_t size) {
    this->engine = &engine;
    render_target_initialized = false;

    vertex_shader = ShaderModule(engine.device, "shaders/equirect_to_cubemap.vert.spv");
    fragment_shader = ShaderModule(engine.device, "shaders/equirect_to_cubemap.frag.spv");

    VulkanVertexLayout vertex_layout;
    LayoutInitializer layout_initializer = vertex_layout.get_initializer();
    layout_initializer.add(AttrFormat::FLOAT4); // position

    uniform_buffer = VideoBuffer(engine, sizeof(EquirectToCubemapPassUniform));

    DescriptorSetBundleBuilder builder;
    builder.add_uniform_buffer(0, uniform_buffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.add_combined_image_sampler(1, VK_SHADER_STAGE_FRAGMENT_BIT);
    descriptor_set_bundle = builder.create(engine.device);

    render_pass = RenderPassBuilder()
        .set_color_attachment(
            RenderPassBuilder::make_offscreen_color_attachment(VK_FORMAT_R16G16B16A16_SFLOAT)
        )
        .set_depth_attachment(
            RenderPassBuilder::make_offscreen_depth_attachment(
                vulkan_utils::find_depth_format(engine.physicalDevice)
            )
        )
        .create(engine.device);

    render_target.create_color_and_depth(
        engine,
        render_pass,
        size,
        size,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        vulkan_utils::find_depth_format(engine.physicalDevice)
    );

    VkExtent2D extent{ size, size };
    pipeline.create(engine, render_pass, extent, descriptor_set_bundle, vertex_layout, vertex_shader, fragment_shader);
}

void EquirectToCubemapPass::render(
    Mesh& cube_mesh,
    Texture2D& hdr_texture,
    Cubemap& output_cubemap
) {
    if (engine->commandPool == VK_NULL_HANDLE) throw std::runtime_error("commandPool null");
    if (engine->graphicsQueue == VK_NULL_HANDLE) throw std::runtime_error("graphicsQueue null");
    if (render_target.color_image == VK_NULL_HANDLE) throw std::runtime_error("render_target.color_image null");
    if (render_target.depth_image == VK_NULL_HANDLE) throw std::runtime_error("render_target.depth_image null");
    if (render_target.framebuffer == VK_NULL_HANDLE) throw std::runtime_error("render_target.framebuffer null");
    if (output_cubemap.image == VK_NULL_HANDLE) throw std::runtime_error("output_cubemap.image null");

    if (!engine) {
        throw std::runtime_error("EquirectToCubemapPass::render(): engine == nullptr");
    }
    VkCommandBuffer cmd = begin_one_time_commands(*engine);

    if (cmd == VK_NULL_HANDLE) {
        throw std::runtime_error("EquirectToCubemapPass: cmd was VK_NULL_HANDLE");
    }

    static const glm::mat4 capture_proj = glm::perspective(
        glm::radians(90.0f), 1.0f, 0.1f, 10.0f
    );

    static const std::array<glm::mat4, 6> capture_views = {
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };

    update_hdr_descriptor(
        engine->device,
        descriptor_set_bundle.descriptor_set.descriptor_set,
        hdr_texture
    );

    // One-time init for the reusable offscreen target.
    // This assumes you want to keep using the target across faces/renders.
    if (!render_target_initialized) {
        cmd_image_barrier(
            cmd,
            render_target.color_image,
            0,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            0, 1,
            0, 1
        );

        VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (has_stencil_component(render_target.depth_format)) {
            depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        cmd_image_barrier(
            cmd,
            render_target.depth_image,
            0,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            depth_aspect,
            0, 1,
            0, 1
        );

        render_target_initialized = true;
    }

    // Put the whole cubemap into TRANSFER_DST so we can copy base level
    // and later generate mipmaps in-place.
    cmd_image_barrier(
        cmd,
        output_cubemap.image,
        VK_ACCESS_SHADER_READ_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        0, output_cubemap.mip_levels,
        0, 6
    );

    for (uint32_t face = 0; face < 6; ++face) {
        EquirectToCubemapPassUniform ubo{};
        ubo.proj = capture_proj;
        ubo.view = capture_views[face];
        uniform_buffer.update_data(&ubo, sizeof(ubo));

        VkClearValue clear_values[2]{};
        clear_values[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clear_values[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo rp_info{};
        rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_info.renderPass = render_pass.render_pass;
        rp_info.framebuffer = render_target.framebuffer;
        rp_info.renderArea.offset = {0, 0};
        rp_info.renderArea.extent = {render_target.width, render_target.height};
        rp_info.clearValueCount = 2;
        rp_info.pClearValues = clear_values;

        vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(render_target.width);
        viewport.height = static_cast<float>(render_target.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {render_target.width, render_target.height};
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline.pipeline_layout,
            0,
            1,
            &descriptor_set_bundle.descriptor_set.descriptor_set,
            0,
            nullptr
        );

        VkBuffer vertex_buffers[] = { cube_mesh.vertex_buffer.buffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, offsets);
        vkCmdBindIndexBuffer(cmd, cube_mesh.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(cmd, cube_mesh.num_indices, 1, 0, 0, 0);

        vkCmdEndRenderPass(cmd);

        // Offscreen color target -> copy source
        cmd_image_barrier(
            cmd,
            render_target.color_image,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            0, 1,
            0, 1
        );

        VkImageCopy copy{};
        copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.srcSubresource.mipLevel = 0;
        copy.srcSubresource.baseArrayLayer = 0;
        copy.srcSubresource.layerCount = 1;
        copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.dstSubresource.mipLevel = 0;
        copy.dstSubresource.baseArrayLayer = face;
        copy.dstSubresource.layerCount = 1;
        copy.extent.width = render_target.width;
        copy.extent.height = render_target.height;
        copy.extent.depth = 1;

        vkCmdCopyImage(
            cmd,
            render_target.color_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            output_cubemap.image,      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &copy
        );

        // Prepare the reusable offscreen target for the next face
        cmd_image_barrier(
            cmd,
            render_target.color_image,
            VK_ACCESS_TRANSFER_READ_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            0, 1,
            0, 1
        );
    }

    cmd_generate_cubemap_mips(cmd, output_cubemap);

    end_one_time_commands(*engine, cmd);
}

void EquirectToCubemapPass::destroy() {
    render_target.destroy();

    // Call these only if your wrappers actually expose destroy()
    // If they are RAII-only, leave them alone.
    // render_pass.destroy();
    // pipeline.destroy();
    // descriptor_set_bundle.destroy();
    // uniform_buffer.destroy();
    // vertex_shader.destroy();
    // fragment_shader.destroy();

    engine = nullptr;
    render_target_initialized = false;
}