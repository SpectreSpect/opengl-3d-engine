#include "equirect_to_cubemap_pass.h"

#include <array>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>

#include "../../mesh.h"
#include "../vulkan_utils.h"

// #include "../../math_utils.h"


EquirectToCubemapPass::EquirectToCubemapPass(VulkanEngine& engine) {
    create(engine);
}

void EquirectToCubemapPass::create(VulkanEngine& engine) {
    this->engine = &engine;
    // Texture2D equirectangular_map = Texture2D(engine, "assets/hdr/st_peters_square_night_4k.hdr",
    //             Texture2D::Wrap::Repeat,
    //             Texture2D::MagFilter::Linear,
    //             Texture2D::MinFilter::LinearMipmapLinear,
    //             true,   // sRGB
    //             true    // flipY
    // );

    // Cubemap cubemap;
    // cubemap.createEmpty(engine, cubemap_face_size);
    // cubemap.transition_to_general_layout(engine);

    command_pool.create(engine.device, engine.physicalDevice);
    command_buffer.create(command_pool);

    equirect_to_cubemap_cs.create(engine.device, "shaders/equirect_to_cubemap.comp.spv");
    equirect_to_cubemap_uniform_buffer.create(engine, sizeof(EquirectToCubemapUniform));



    DescriptorSetBundleBuilder builder = DescriptorSetBundleBuilder();
    builder.add_uniform_buffer(0, equirect_to_cubemap_uniform_buffer, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.add_combined_image_sampler(1, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.add_image_storage(2, VK_SHADER_STAGE_COMPUTE_BIT);
    descriptor_set_bundle = builder.create(engine.device);

    pipeline.create(engine.device, descriptor_set_bundle, equirect_to_cubemap_cs);

    fence = Fence(engine.device);

    // uint32_t x_groups = math_utils::div_up_u32(equirect_to_cubemap_uniform.image_width, 256);
    // uint32_t y_groups = equirect_to_cubemap_uniform.image_height;
    // uint32_t z_groups = equirect_to_cubemap_uniform.num_layers;
    
    // command_buffer.begin();
    // command_buffer.bind_pipeline(compute_pipeline);
    // command_buffer.dispatch(x_groups, y_groups, z_groups);
    // // command_buffer.memory_barrier(temp_storage_buffer);
    // command_buffer.end();

    // Fence fence(engine.device);

    // command_buffer.submit(fence);

    // fence.wait_for_fence();

    // cubemap.transition_to_shader_read_only_layout(engine);
}

void EquirectToCubemapPass::generate(Texture2D& equirectangular_map, Cubemap& output_cubemap) {
    if (!this->engine)
        throw std::runtime_error("engine was null");

    // output_cubemap.destroy();
    output_cubemap.transition_to_general_layout(*this->engine);

    uint32_t face_size = output_cubemap.faceSize();

    EquirectToCubemapUniform equirect_to_cubemap_uniform{};
    equirect_to_cubemap_uniform.image_width = face_size;
    equirect_to_cubemap_uniform.image_height = face_size;
    equirect_to_cubemap_uniform.num_layers = 6;
    equirect_to_cubemap_uniform_buffer.update_data(&equirect_to_cubemap_uniform, sizeof(EquirectToCubemapUniform));

    descriptor_set_bundle.bind_combined_image_sampler(1, equirectangular_map);
    descriptor_set_bundle.bind_image_storage(2, output_cubemap);

    uint32_t x_groups = vulkan_utils::div_up_u32(equirect_to_cubemap_uniform.image_width, 256);
    uint32_t y_groups = equirect_to_cubemap_uniform.image_height;
    uint32_t z_groups = equirect_to_cubemap_uniform.num_layers;
    
    command_buffer.begin();
    command_buffer.bind_pipeline(pipeline);
    command_buffer.dispatch(x_groups, y_groups, z_groups);
    // command_buffer.memory_barrier(temp_storage_buffer);
    command_buffer.end();

    command_buffer.submit(fence);

    fence.wait_for_fence();

    output_cubemap.transition_to_shader_read_only_layout(*this->engine);
}