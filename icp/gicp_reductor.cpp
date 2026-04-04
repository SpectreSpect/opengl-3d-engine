#include "gicp_reductor.h"

GICPReductor::GICPReductor(VulkanEngine& engine)
    : engine(&engine)
{
    compute_queue_family_id = vulkan_utils::find_compute_queue_family(engine.physicalDevice);
    vkGetDeviceQueue(engine.device, compute_queue_family_id, 0, &compute_queue);

    command_pool.create(engine.device, engine.physicalDevice, compute_queue_family_id, compute_queue);
    command_buffer.create(command_pool);

    shader_module.create(engine.device, "shaders/icp/gicp_reduce.comp.spv");
    uniform_buffer.create(engine, sizeof(GICPReductorUniform));

    DescriptorSetBundleBuilder builder;
    builder.add_uniform_buffer(0, uniform_buffer, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.add_storage_buffer(1, VK_SHADER_STAGE_COMPUTE_BIT); // partial source buffer
    builder.add_storage_buffer(2, VK_SHADER_STAGE_COMPUTE_BIT); // partial output buffer

    descriptor_set_bundle = builder.create(engine.device);
    pipeline.create(engine.device, descriptor_set_bundle, shader_module);

    fence = Fence(engine.device);
}

GICPReductor::GICPReductor(GICPReductor&& other) noexcept
    : engine(std::exchange(other.engine, nullptr)),
      pipeline(std::move(other.pipeline)),
      descriptor_set_bundle(std::move(other.descriptor_set_bundle)),
      fence(std::move(other.fence)),
      uniform_buffer(std::move(other.uniform_buffer)),
      shader_module(std::move(other.shader_module)),
      compute_queue_family_id(std::exchange(other.compute_queue_family_id, 0)),
      compute_queue(std::exchange(other.compute_queue, VK_NULL_HANDLE)),
      command_pool(std::move(other.command_pool)),
      command_buffer(std::move(other.command_buffer))
{
    pipeline.descriptor_set_bundle = &descriptor_set_bundle;
    command_buffer.command_pool = &command_pool;
    command_buffer.device = command_pool.device;
}

GICPReductor& GICPReductor::operator=(GICPReductor&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    engine = std::exchange(other.engine, nullptr);
    pipeline = std::move(other.pipeline);
    descriptor_set_bundle = std::move(other.descriptor_set_bundle);
    fence = std::move(other.fence);
    uniform_buffer = std::move(other.uniform_buffer);
    shader_module = std::move(other.shader_module);
    compute_queue_family_id = std::exchange(other.compute_queue_family_id, 0);
    compute_queue = std::exchange(other.compute_queue, VK_NULL_HANDLE);
    command_pool = std::move(other.command_pool);
    command_buffer = std::move(other.command_buffer);

    pipeline.descriptor_set_bundle = &descriptor_set_bundle;
    command_buffer.command_pool = &command_pool;
    command_buffer.device = command_pool.device;

    return *this;
}

uint32_t GICPReductor::reduce_step(VideoBuffer& partial_src, VideoBuffer& partial_dst, uint32_t input_count) {
    if (!engine) {
        throw std::runtime_error("engine was null");
    }

    GICPReductorUniform uniform_data{};
    uniform_data.input_count = input_count;
    uniform_buffer.update_data(&uniform_data, sizeof(GICPReductorUniform));

    descriptor_set_bundle.bind_storage_buffer(1, partial_src);
    descriptor_set_bundle.bind_storage_buffer(2, partial_dst);

    uint32_t x_groups = vulkan_utils::div_up_u32(input_count, 64);

    command_buffer.begin();
    command_buffer.bind_pipeline(pipeline);
    command_buffer.dispatch(x_groups, 1, 1);
    command_buffer.end();

    command_buffer.submit_and_wait(compute_queue, fence);

    return x_groups;
}

GICPReductor::GICPPartial GICPReductor::reduce(VideoBuffer& partial_src, VideoBuffer& partial_dst, uint32_t input_count) {
    VideoBuffer* src = &partial_src;
    VideoBuffer* dst = &partial_dst;

    while (input_count > 1) {
        input_count = reduce_step(*src, *dst, input_count);
        std::swap(src, dst);
    }

    GICPPartial output{};
    src->read_subdata(0, &output, sizeof(GICPPartial));
    return output;
}