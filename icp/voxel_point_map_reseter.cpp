#include "voxel_point_map_reseter.h"


void VoxelPointMapReseter::create(VulkanEngine& engine) {
    this->engine = &engine;
    compute_queue_family_id = vulkan_utils::find_compute_queue_family(engine.physicalDevice);
    vkGetDeviceQueue(engine.device, compute_queue_family_id, 0, &compute_queue);

    command_pool.create(engine.device, engine.physicalDevice, compute_queue_family_id, compute_queue);
    command_buffer.create(command_pool);

    shader_module.create(engine.device, "shaders/icp/reset_voxel_point_map.comp.spv");
    uniform_buffer.create(engine, sizeof(ReseterUniform));
    // output_buffer.create(engine, sizeof(OutputBuffer));

    DescriptorSetBundleBuilder builder = DescriptorSetBundleBuilder();
    builder.add_uniform_buffer(0, uniform_buffer, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.add_storage_buffer(2, VK_SHADER_STAGE_COMPUTE_BIT); // MapPointCountBuffer
    builder.add_storage_buffer(4, VK_SHADER_STAGE_COMPUTE_BIT); // VoxelHashTableBuffer
    descriptor_set_bundle = builder.create(engine.device);

    pipeline.create(engine.device, descriptor_set_bundle, shader_module);

    fence = Fence(engine.device);
}

void VoxelPointMapReseter::reset(VoxelPointMap& voxel_point_map) {
    if (!this->engine)
        throw std::runtime_error("engine was null");
        
    ReseterUniform uniform_data{};
    uniform_data.num_hash_table_slots = voxel_point_map.num_hash_table_slots;
    uniform_buffer.update_data(&uniform_data, sizeof(ReseterUniform));

    descriptor_set_bundle.bind_storage_buffer(2, voxel_point_map.map_point_count_buffer);
    descriptor_set_bundle.bind_storage_buffer(4, voxel_point_map.map_hash_table_buffer);

    // descriptor_set_bundle.bind_storage_buffer(1, source_point_cloud.instance_buffer);
    // descriptor_set_bundle.bind_storage_buffer(2, target_point_cloud.instance_buffer);

    // descriptor_set_bundle.bind_storage_buffer(3, source_normal_buffer);
    // descriptor_set_bundle.bind_storage_buffer(4, target_normal_buffer);

    // descriptor_set_bundle.bind_storage_buffer(5, output_buffer);
    
    // descriptor_set_bundle.bind_image_storage(1, brdf_lut_texture);

    uint32_t x_groups = vulkan_utils::div_up_u32(uniform_data.num_hash_table_slots, 256);
    
    command_buffer.begin();

    command_buffer.bind_pipeline(pipeline);
    command_buffer.dispatch(x_groups, 1, 1);

    // command_buffer.memory_barrier(temp_storage_buffer);
    command_buffer.end();

    command_buffer.submit_and_wait(compute_queue, fence);

    voxel_point_map.map_point_count = 0;

    // OutputBuffer output_data{};
    // output_buffer.read_subdata(0, &output_data, sizeof(output_data));

    // source_point_cloud.position = output_data.position;
    // source_point_cloud.rotation = output_data.rotation;
}