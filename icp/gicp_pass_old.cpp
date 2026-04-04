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
    // builder.add_uniform_buffer(0, uniform_buffer, VK_SHADER_STAGE_COMPUTE_BIT);
    // builder.add_storage_buffer(1, VK_SHADER_STAGE_COMPUTE_BIT); // source point cloud
    // builder.add_storage_buffer(2, VK_SHADER_STAGE_COMPUTE_BIT); // target point cloud
    // builder.add_storage_buffer(3, VK_SHADER_STAGE_COMPUTE_BIT); // source normals
    // builder.add_storage_buffer(4, VK_SHADER_STAGE_COMPUTE_BIT); // target normals
    // builder.add_storage_buffer(5, output_buffer, VK_SHADER_STAGE_COMPUTE_BIT); // output buffer

    builder.add_uniform_buffer(0, uniform_buffer, VK_SHADER_STAGE_COMPUTE_BIT);
    
    builder.add_storage_buffer(1, VK_SHADER_STAGE_COMPUTE_BIT); // source point buffer
    builder.add_storage_buffer(2, VK_SHADER_STAGE_COMPUTE_BIT); // source normal buffer
    builder.add_storage_buffer(3, VK_SHADER_STAGE_COMPUTE_BIT); // map point count buffer
    builder.add_storage_buffer(4, VK_SHADER_STAGE_COMPUTE_BIT); // map point buffer
    builder.add_storage_buffer(5, VK_SHADER_STAGE_COMPUTE_BIT); // map normal buffer
    builder.add_storage_buffer(6, VK_SHADER_STAGE_COMPUTE_BIT); // output storage buffer
    builder.add_storage_buffer(7, VK_SHADER_STAGE_COMPUTE_BIT); // voxel hash table buffer
    builder.add_storage_buffer(8, VK_SHADER_STAGE_COMPUTE_BIT); // partial ouput buffer


    // builder.add_storage_buffer(2, VK_SHADER_STAGE_COMPUTE_BIT); // target point cloud
    // builder.add_storage_buffer(3, VK_SHADER_STAGE_COMPUTE_BIT); // source normals
    // builder.add_storage_buffer(4, VK_SHADER_STAGE_COMPUTE_BIT); // target normals
    // builder.add_storage_buffer(5, output_buffer, VK_SHADER_STAGE_COMPUTE_BIT); // output buffer
    descriptor_set_bundle = builder.create(engine.device);

    pipeline.create(engine.device, descriptor_set_bundle, shader_module);

    fence = Fence(engine.device);

    // partial_src = VideoBuffer(engine, sizeof(GICPReductor::GICPPartial) * max_partial_count);
    // partial_dst = VideoBuffer(engine, sizeof(GICPReductor::GICPPartial) * max_partial_count);
    partial_src.create(engine, sizeof(GICPReductor::GICPPartial) * max_partial_count);
    partial_dst.create(engine, sizeof(GICPReductor::GICPPartial) * max_partial_count);
    reductor = GICPReductor(engine);
}


// void GICPPass::step(PointCloud& source_point_cloud, PointCloud& target_point_cloud, VideoBuffer& source_normal_buffer, VideoBuffer& target_normal_buffer) {
//     if (!this->engine)
//         throw std::runtime_error("engine was null");
    
//     // VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

//     // VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT |
//     //                           VK_IMAGE_USAGE_STORAGE_BIT |
//     //                           VK_IMAGE_USAGE_TRANSFER_DST_BIT |
//     //                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
//     //                           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        
//     GICPPassUniform uniform_data{};
//     uniform_data.num_source_points = source_point_cloud.num_instances;
//     uniform_data.num_target_points = target_point_cloud.num_instances;
//     uniform_data.position = glm::vec4(source_point_cloud.position, 1.0f);
//     uniform_data.rotation = glm::vec4(source_point_cloud.rotation, 1.0f);
//     uniform_buffer.update_data(&uniform_data, sizeof(GICPPassUniform));

//     descriptor_set_bundle.bind_storage_buffer(1, source_point_cloud.get_instance_buffer());
//     descriptor_set_bundle.bind_storage_buffer(2, target_point_cloud.get_instance_buffer());

//     descriptor_set_bundle.bind_storage_buffer(3, source_normal_buffer);
//     descriptor_set_bundle.bind_storage_buffer(4, target_normal_buffer);

//     descriptor_set_bundle.bind_storage_buffer(5, output_buffer);
    
//     // descriptor_set_bundle.bind_image_storage(1, brdf_lut_texture);

//     uint32_t x_groups = vulkan_utils::div_up_u32(source_point_cloud.num_instances, 256);
    
//     command_buffer.begin();

//     command_buffer.bind_pipeline(pipeline);
//     command_buffer.dispatch(x_groups, 1, 1);

//     // command_buffer.memory_barrier(temp_storage_buffer);
//     command_buffer.end();

//     command_buffer.submit_and_wait(compute_queue, fence);

//     OutputBuffer output_data{};
//     output_buffer.read_subdata(0, &output_data, sizeof(output_data));

//     source_point_cloud.position = output_data.position;
//     source_point_cloud.rotation = output_data.rotation;

//     // std::cout << output_data.test_output.x << " " << output_data.test_output.y << " " << output_data.test_output.z << " " << output_data.test_output.w << std::endl;
// }

void GICPPass::step(VoxelPointMap& voxel_point_map, PointCloud& source_point_cloud, VideoBuffer& source_normal_buffer) {
    if (!this->engine)
        throw std::runtime_error("engine was null");
        
    GICPPassUniform uniform_data{};
    uniform_data.position = glm::vec4(source_point_cloud.position, 1.0f);
    uniform_data.rotation = glm::vec4(source_point_cloud.rotation, 1.0f);
    uniform_data.num_source_points = source_point_cloud.num_instances;
    uniform_data.num_target_points = voxel_point_map.map_point_count;
    uniform_data.num_hash_table_slots = voxel_point_map.num_hash_table_slots;
    
    uniform_buffer.update_data(&uniform_data, sizeof(GICPPassUniform));

    descriptor_set_bundle.bind_storage_buffer(1, source_point_cloud.get_instance_buffer());
    descriptor_set_bundle.bind_storage_buffer(2, source_normal_buffer);
    descriptor_set_bundle.bind_storage_buffer(3, voxel_point_map.map_point_count_buffer);
    descriptor_set_bundle.bind_storage_buffer(4, voxel_point_map.map_point_buffer);
    descriptor_set_bundle.bind_storage_buffer(5, voxel_point_map.map_normal_buffer);
    descriptor_set_bundle.bind_storage_buffer(6, output_buffer);
    descriptor_set_bundle.bind_storage_buffer(7, voxel_point_map.map_hash_table_buffer);
    descriptor_set_bundle.bind_storage_buffer(8, partial_src);

    uint32_t x_groups = vulkan_utils::div_up_u32(source_point_cloud.num_instances, 256);
    
    command_buffer.begin();

    command_buffer.bind_pipeline(pipeline);
    command_buffer.dispatch(x_groups, 1, 1);

    // command_buffer.memory_barrier(temp_storage_buffer);
    command_buffer.end();

    command_buffer.submit_and_wait(compute_queue, fence);

    // OutputBuffer output_data{};
    // output_buffer.read_subdata(0, &output_data, sizeof(output_data));

    // source_point_cloud.position = output_data.position;
    // source_point_cloud.rotation = output_data.rotation;

    GICPReductor::GICPPartial result = reductor.reduce(partial_src, partial_dst, source_point_cloud.num_instances);

    const float max_corr_dist    = 10.0;
    const float max_corr_dist_sq = max_corr_dist * max_corr_dist;
    const float max_rot          = glm::radians(5.0);
    const float max_trans        = 5.0;
    const float gicp_eps         = 1e-3;
    const float min_normal_dot   = -1.0;
    const glm::vec3  source_scale     = glm::vec3(1.0);


    if (result.valid_count < 6) {
        std::cout << "valid_count was less than 6" << std::endl;
        return;
    }

    double rmse = std::sqrt(result.total_weighted_sq_error / double(result.valid_count));

    const double lambda = 1e-6;
    for (int i = 0; i < 6; ++i) {
        result.H[i][i] += lambda;
    }

    double delta[6] = {};

    bool ok = GICP::solve_6x6(result.H, result.g, delta);
    if (!ok) {
        std::cout << "faield solve_6x6" << std::endl;
        return;
    }

    glm::vec3 omega(
        (float)delta[0],
        (float)delta[1],
        (float)delta[2]
    );

    glm::vec3 v(
        (float)delta[3],
        (float)delta[4],
        (float)delta[5]
    );

    float omega_len = glm::length(omega);
    if (omega_len > max_rot) {
        omega *= max_rot / omega_len;
    }

    float v_len = glm::length(v);
    if (v_len > max_trans) {
        v *= max_trans / v_len;
    }

    glm::mat3 dR = GICP::omega_to_mat3(omega);

    glm::mat3 R_src = GICP::euler_xyz_to_mat3(source_point_cloud.rotation);
    glm::mat3 R_src_new = dR * R_src;
    glm::vec3 t_src_new = dR * source_point_cloud.position + v;

    source_point_cloud.position = t_src_new;
    source_point_cloud.rotation = GICP::mat3_to_euler_xyz(R_src_new);
}