#include "compute_program.h"

void ComputeProgram::create(VulkanEngine& engine, ShaderModule& shader, uint32_t uniform_buffer_size_bytes, DescriptorSetBundle& descriptor_set_bundle) {
    this->engine = &engine;
    this->shader = &shader;
    this->descriptor_set_bundle = &descriptor_set_bundle;

    compute_queue_family_id = vulkan_utils::find_compute_queue_family(engine.physicalDevice);
    vkGetDeviceQueue(engine.device, compute_queue_family_id, 0, &compute_queue);

    command_pool.create(engine.device, engine.physicalDevice, compute_queue_family_id, compute_queue);
    command_buffer.create(command_pool);

    // generate_prefilter_map_cs.create(engine.device, "shaders/generate_prefilter_map.comp.spv");
    uniform_buffer.create(engine, uniform_buffer_size_bytes);

    // DescriptorSetBundleBuilder builder = DescriptorSetBundleBuilder();
    // builder.add_uniform_buffer(0, uniform_buffer, VK_SHADER_STAGE_COMPUTE_BIT);
    // builder.add_combined_image_sampler(1, VK_SHADER_STAGE_COMPUTE_BIT);
    // builder.add_image_storage(2, VK_SHADER_STAGE_COMPUTE_BIT);
    // descriptor_set_bundle = builder.create(engine.device);
    pipeline.create(engine.device, descriptor_set_bundle, shader);
    fence = Fence(engine.device);
}

void ComputeProgram::update_uniform(void* data, uint32_t size_bytes) {
    uniform_buffer.update_data(&data, size_bytes);
}

void ComputeProgram::begin() {
    if (!ended)
        throw std::runtime_error("ComputeProgram::begin: compute program has not been ended");
    
    command_buffer.begin();
    started = true;
    ended = false;
}

void ComputeProgram::end() {
    if (!started)
        throw std::runtime_error("ComputeProgram::end: compute program has not been started");

    command_buffer.end();
    ended = true;
}

void ComputeProgram::end_submit_wait() {
    end();
    command_buffer.submit_and_wait(compute_queue, fence);
}

void ComputeProgram::dispatch(uint32_t x_groups, uint32_t y_groups, uint32_t z_groups) {
    if (!started)
        throw std::runtime_error("ComputeProgram::end: compute program has not been started");

    // Cubemap cubemap;
    // cubemap.create(*engine, face_size, 1, usage, VK_FORMAT_R32G32B32A32_SFLOAT);

    // IrradianceMapGeneratorUniform uniform_data{};
    // uniform_data.image_width = face_size;
    // uniform_data.image_height = face_size;
    // uniform_data.num_layers = 6;
    // uniform_buffer.update_data(&uniform_data, sizeof(IrradianceMapGeneratorUniform));

    // descriptor_set_bundle.bind_combined_image_sampler(1, environment_map);
    // descriptor_set_bundle.bind_image_storage(2, cubemap);

    // uint32_t x_groups = vulkan_utils::div_up_u32(face_size, 16);
    // uint32_t y_groups = vulkan_utils::div_up_u32(face_size, 16);
    // uint32_t z_groups = uniform_data.num_layers;
    
    // command_buffer.begin();

    // cubemap.image_resource.transition_layout(
    //     command_buffer,
    //     VK_IMAGE_LAYOUT_GENERAL,
    //     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    //     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //     0,
    //     VK_ACCESS_SHADER_WRITE_BIT
    // );

    command_buffer.bind_pipeline(pipeline);
    command_buffer.dispatch(x_groups, y_groups, z_groups);

    // cubemap.image_resource.transition_layout(
    //     command_buffer,
    //     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    //     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    //     VK_ACCESS_SHADER_WRITE_BIT,
    //     VK_ACCESS_SHADER_READ_BIT
    // );

    // command_buffer.memory_barrier(temp_storage_buffer);
    // command_buffer.end();

    // command_buffer.submit_and_wait(compute_queue, fence);
}

    // Cubemap cubemap;
    // cubemap.create(*engine, face_size, 1, usage, VK_FORMAT_R32G32B32A32_SFLOAT);

    // IrradianceMapGeneratorUniform uniform_data{};
    // uniform_data.image_width = face_size;
    // uniform_data.image_height = face_size;
    // uniform_data.num_layers = 6;
    // uniform_buffer.update_data(&uniform_data, sizeof(IrradianceMapGeneratorUniform));

    // descriptor_set_bundle.bind_combined_image_sampler(1, environment_map);
    // descriptor_set_bundle.bind_image_storage(2, cubemap);

    // uint32_t x_groups = vulkan_utils::div_up_u32(face_size, 16);
    // uint32_t y_groups = vulkan_utils::div_up_u32(face_size, 16);
    // uint32_t z_groups = uniform_data.num_layers;
    
    // command_buffer.begin();

    // cubemap.image_resource.transition_layout(
    //     command_buffer,
    //     VK_IMAGE_LAYOUT_GENERAL,
    //     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    //     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //     0,
    //     VK_ACCESS_SHADER_WRITE_BIT
    // );

    // command_buffer.bind_pipeline(pipeline);
    // command_buffer.dispatch(x_groups, y_groups, z_groups);

    // cubemap.image_resource.transition_layout(
    //     command_buffer,
    //     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    //     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    //     VK_ACCESS_SHADER_WRITE_BIT,
    //     VK_ACCESS_SHADER_READ_BIT
    // );

    // // command_buffer.memory_barrier(temp_storage_buffer);
    // command_buffer.end();

    // command_buffer.submit_and_wait(compute_queue, fence);