#include "point_cloud_pass.h"

void PointCloudPass::create(VulkanEngine& engine) {
    this->engine = &engine;
    
    vertex_shader = ShaderModule(engine.device, "shaders/point_cloud/point_cloud.vert.spv");
    fragment_shader = ShaderModule(engine.device, "shaders/point_cloud/point_cloud.frag.spv");
    
    VulkanVertexLayout vertex_layout;
    LayoutInitializer layout_initializer = vertex_layout.get_initializer();
    layout_initializer.add_binding(VK_VERTEX_INPUT_RATE_VERTEX, sizeof(PointQuadVertex));
    layout_initializer.add_binding(VK_VERTEX_INPUT_RATE_INSTANCE, sizeof(PointInstance));
    
    layout_initializer.add(AttrFormat::FLOAT2, 0); // Quad corners
    layout_initializer.add(AttrFormat::FLOAT4, 1); // Instance position
    layout_initializer.add(AttrFormat::FLOAT4, 1); // Instance color

    uniform_buffer = VideoBuffer(engine, sizeof(PointCloudUniform));

    DescriptorSetBundleBuilder builder = DescriptorSetBundleBuilder();
    
    builder.add_uniform_buffer(0, uniform_buffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    descriptor_set_bundle = builder.create(engine.device);

    pipeline = GraphicsPipeline(engine, descriptor_set_bundle, vertex_layout, vertex_shader, fragment_shader);

    const float quad_corners[] = { // vertex buffer
        -1.0f, -1.0f,  // v0
        -1.0f, +1.0f,  // v1
        +1.0f, -1.0f,  // v2
        +1.0f, +1.0f   // v3
    };

    const unsigned int quad_indices[] = { // index_buffer
        0, 1, 2,
        2, 1, 3
    };

    quad_mesh = Mesh(engine, (void*)quad_corners, (uint32_t)sizeof(quad_corners), (unsigned int*)quad_indices, (uint32_t)sizeof(quad_indices));
}

void PointCloudPass::render(PointCloud& point_cloud, Camera& camera) {
    if (!engine)
        throw std::runtime_error("PointCloudPass::render: engine was null");
    
    if (point_cloud.num_instances <= 0)
        return;
    
    glm::mat4 model = point_cloud.get_model_matrix();

    PointCloudUniform uniform_data{};

    float aspect = float(engine->swapchainExtent.width) / float(engine->swapchainExtent.height);

    uniform_data.view = camera.get_view_matrix();
    uniform_data.proj = camera.get_projection_matrix(aspect);
    uniform_data.viewport = {engine->swapchainExtent.width, engine->swapchainExtent.height};


    uniform_buffer.update_data(&uniform_data, sizeof(uniform_data));


    engine->bind_pipeline(pipeline);
    // engine->bind_vertex_buffer(quad_mesh.vertex_buffer);
    engine->bind_vertex_and_instance_buffers(quad_mesh.vertex_buffer, point_cloud.instance_buffer);
    engine->bind_index_buffer(quad_mesh.index_buffer);


    PointCloudPushConstants pc{};

    pc.model = model;
    pc.point_size_px = point_cloud.point_size_px;
    pc.point_size_world = point_cloud.point_size_world;
    pc.screen_space_size = point_cloud.screen_space_size;

    vkCmdPushConstants(
        engine->currentCommandBuffer,
        pipeline.pipeline_layout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(pc),
        &pc
    );

    vkCmdDrawIndexed(engine->currentCommandBuffer, quad_mesh.num_indices, point_cloud.num_instances, 0, 0, 0);
}