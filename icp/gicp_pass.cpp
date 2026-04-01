#include "gicp_pass.h"


void GICPPass::create(VulkanEngine& engine) {
    this->engine = &engine;
    compute_queue_family_id = vulkan_utils::find_compute_queue_family(engine.physicalDevice);
    vkGetDeviceQueue(engine.device, compute_queue_family_id, 0, &compute_queue);

    command_pool.create(engine.device, engine.physicalDevice, compute_queue_family_id, compute_queue);
    command_buffer.create(command_pool);

    generate_brdf_lut_cs.create(engine.device, "icp/gicp_step.comp.spv");
    uniform_buffer.create(engine, sizeof(GICPPassUniform));

    DescriptorSetBundleBuilder builder = DescriptorSetBundleBuilder();
    builder.add_uniform_buffer(0, uniform_buffer, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.add_image_storage(1, VK_SHADER_STAGE_COMPUTE_BIT);
    descriptor_set_bundle = builder.create(engine.device);

    pipeline.create(engine.device, descriptor_set_bundle, generate_brdf_lut_cs);

    fence = Fence(engine.device);
}


void GICPPass::step() {

    if (!this->engine)
        throw std::runtime_error("engine was null");
    
    VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

    // VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT |
    //                           VK_IMAGE_USAGE_STORAGE_BIT |
    //                           VK_IMAGE_USAGE_TRANSFER_DST_BIT |
    //                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
    //                           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        
    GICPPassUniform uniform_data{};
    uniform_data.num_points = 100;
    uniform_buffer.update_data(&uniform_data, sizeof(GICPPassUniform));

    uint32_t x_count = uniform_data.num_points;

    // descriptor_set_bundle.bind_image_storage(1, brdf_lut_texture);

    uint32_t x_groups = vulkan_utils::div_up_u32(x_count, 256);
    
    command_buffer.begin();

    command_buffer.bind_pipeline(pipeline);
    command_buffer.dispatch(x_groups, 1, 1);


    // command_buffer.memory_barrier(temp_storage_buffer);
    command_buffer.end();

    command_buffer.submit_and_wait(compute_queue, fence);
}