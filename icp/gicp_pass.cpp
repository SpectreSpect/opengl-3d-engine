#include "gicp_pass.h"


void GICPPass::create(VulkanEngine& engine) {
    this->engine = &engine;
    compute_queue_family_id = vulkan_utils::find_compute_queue_family(engine.physicalDevice);
    vkGetDeviceQueue(engine.device, compute_queue_family_id, 0, &compute_queue);

    command_pool.create(engine.device, engine.physicalDevice, compute_queue_family_id, compute_queue);
    command_buffer.create(command_pool);

    shader_module.create(engine.device, "shaders/icp/gicp_step.comp.spv");
    uniform_buffer.create(engine, sizeof(GICPPassUniform));
    output_buffer.create(engine, sizeof(OutputBuffer));

    DescriptorSetBundleBuilder builder = DescriptorSetBundleBuilder();
    builder.add_uniform_buffer(0, uniform_buffer, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.add_storage_buffer(1, VK_SHADER_STAGE_COMPUTE_BIT); // source point cloud
    builder.add_storage_buffer(2, VK_SHADER_STAGE_COMPUTE_BIT); // target point cloud
    builder.add_storage_buffer(3, VK_SHADER_STAGE_COMPUTE_BIT); // source normals
    builder.add_storage_buffer(4, VK_SHADER_STAGE_COMPUTE_BIT); // target normals
    builder.add_storage_buffer(5, output_buffer, VK_SHADER_STAGE_COMPUTE_BIT); // output buffer
    descriptor_set_bundle = builder.create(engine.device);

    pipeline.create(engine.device, descriptor_set_bundle, shader_module);

    fence = Fence(engine.device);
}


void GICPPass::step(PointCloud& source_point_cloud, PointCloud& target_point_cloud, VideoBuffer& source_normal_buffer, VideoBuffer& target_normal_buffer) {
    if (!this->engine)
        throw std::runtime_error("engine was null");
    
    // VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

    // VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT |
    //                           VK_IMAGE_USAGE_STORAGE_BIT |
    //                           VK_IMAGE_USAGE_TRANSFER_DST_BIT |
    //                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
    //                           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        
    GICPPassUniform uniform_data{};
    uniform_data.num_source_points = source_point_cloud.num_instances;
    uniform_data.num_target_points = target_point_cloud.num_instances;
    uniform_data.position = glm::vec4(source_point_cloud.position, 1.0f);
    uniform_data.rotation = glm::vec4(source_point_cloud.rotation, 1.0f);
    uniform_buffer.update_data(&uniform_data, sizeof(GICPPassUniform));

    descriptor_set_bundle.bind_storage_buffer(1, source_point_cloud.get_instance_buffer());
    descriptor_set_bundle.bind_storage_buffer(2, target_point_cloud.get_instance_buffer());

    descriptor_set_bundle.bind_storage_buffer(3, source_normal_buffer);
    descriptor_set_bundle.bind_storage_buffer(4, target_normal_buffer);

    descriptor_set_bundle.bind_storage_buffer(5, output_buffer);
    
    // descriptor_set_bundle.bind_image_storage(1, brdf_lut_texture);

    uint32_t x_groups = vulkan_utils::div_up_u32(source_point_cloud.num_instances, 256);
    
    command_buffer.begin();

    command_buffer.bind_pipeline(pipeline);
    command_buffer.dispatch(x_groups, 1, 1);

    // command_buffer.memory_barrier(temp_storage_buffer);
    command_buffer.end();

    command_buffer.submit_and_wait(compute_queue, fence);

    OutputBuffer output_data{};
    output_buffer.read_subdata(0, &output_data, sizeof(output_data));

    source_point_cloud.position = output_data.position;
    source_point_cloud.rotation = output_data.rotation;

    // std::cout << output_data.test_output.x << " " << output_data.test_output.y << " " << output_data.test_output.z << " " << output_data.test_output.w << std::endl;
}