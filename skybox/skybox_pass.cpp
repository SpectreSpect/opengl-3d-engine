#include "skybox_pass.h"

void SkyboxPass::create(VulkanEngine& engine) {
    this->engine = &engine;

    command_pool.create(engine.device, engine.physicalDevice);
    command_buffer.create(command_pool);

    skybox_vs.create(engine.device, "shaders/skybox.vert.spv");
    skybox_fs.create(engine.device, "shaders/skybox.frag.spv");

    VulkanVertexLayout vertex_layout;
    LayoutInitializer layout_initializer = vertex_layout.get_initializer();
    layout_initializer.add(AttrFormat::FLOAT4); // position

    uniform_buffer = VideoBuffer(engine, sizeof(SkyboxUniform));

    DescriptorSetBundleBuilder builder = DescriptorSetBundleBuilder();
    builder.add_uniform_buffer(0, uniform_buffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.add_combined_image_sampler(1, VK_SHADER_STAGE_FRAGMENT_BIT); // albedo
    builder.add_combined_image_sampler(2, VK_SHADER_STAGE_FRAGMENT_BIT); // env_map
    descriptor_set_bundle = builder.create(engine.device);

    pipeline = GraphicsPipeline(engine, descriptor_set_bundle, vertex_layout, skybox_vs, skybox_fs);

    // uniform_buffer = VideoBuffer(engine, sizeof(PBRUniform));

    // DescriptorSetBundleBuilder builder = DescriptorSetBundleBuilder();
    // builder.add_uniform_buffer(0, uniform_buffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    // builder.add_combined_image_sampler(1, VK_SHADER_STAGE_FRAGMENT_BIT); // albedo
    // builder.add_combined_image_sampler(2, VK_SHADER_STAGE_FRAGMENT_BIT); // env_map
    // descriptor_set_bundle = builder.create(engine.device);


    // equirect_to_cubemap_uniform_buffer.create(engine, sizeof(EquirectToCubemapUniform));



    // DescriptorSetBundleBuilder builder = DescriptorSetBundleBuilder();
    // builder.add_uniform_buffer(0, equirect_to_cubemap_uniform_buffer, VK_SHADER_STAGE_COMPUTE_BIT);
    // builder.add_combined_image_sampler(1, VK_SHADER_STAGE_COMPUTE_BIT);
    // builder.add_image_storage(2, VK_SHADER_STAGE_COMPUTE_BIT);
    // descriptor_set_bundle = builder.create(engine.device);

    // pipeline.create(engine.device, descriptor_set_bundle, equirect_to_cubemap_cs);

    // fence = Fence(engine.device);
}