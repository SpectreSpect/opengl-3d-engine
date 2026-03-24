#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <iostream>
#include <string>
#include <chrono>
#include <cmath>

// #include "engine3d.h"
// #include "opengl_engine.h"
#include "vulkan_engine.h"
#include <array>
#include <tuple>

// #include "vao.h"
// #include "vbo.h"
// #include "ebo.h"
// #include "vertex_layout.h"
// #include "program.h"
#include "camera.h"
// #include "mesh.h"
// #include "cube.h"
// #include "window.h"
#include "fps_camera_controller.h"
// #include "voxel_engine/chunk.h"
// #include "voxel_engine/voxel_grid.h"
// #include "imgui_layer.h"
// #include "voxel_rastorizator.h"
// #include "ui_elements/triangle_controller.h"
// #include "triangle.h"
// #include "a_star/a_star.h"
// #include "line.h"
// #include "a_star/nonholonomic_a_star.h"
// #include "a_star/reeds_shepp.h"

#include <cstdint>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
// #include "point.h"
// #include "point_cloud/point_cloud_frame.h"
// #include "point_cloud/point_cloud_video.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>
#include <cmath>
#include <cstdint>
// #include "math_utils.h"
// #include "light_source.h"
// #include "circle_cloud.h"
// #include "texture.h"
// #include "cubemap.h"
// #include "skybox.h"
// #include "framebuffer.h"
// #include "sphere.h"
// #include "cube.h"
// #include "quad.h"
// #include "texture_manager.h"
// #include "pbr_skybox.h"
#include <algorithm>
#include <random>
// #include "third_person_camera_controller.h"
// #include "spider.h"
#include "vulkan_window.h"
#include "vulkan/shader_module.h"
#include "vulkan/video_buffer.h"
#include "vulkan/vulkan_vertex_layout.h"
#include "vulkan/graphics_pipeline.h"
#include "vulkan/pbr_renderer.h"
#include "mesh.h"
#include "pbr_uniform.h"
#include "imgui_layer.h"
#include "vulkan/texture2d.h"
#include "vulkan/render_target_2d.h"
#include "vulkan/render_pass.h"
#include "vulkan/cubemap.h"
#include "vulkan/pbr/equirect_to_cubemap_pass.h"
#include "vulkan/command_pool.h"
#include "vulkan/command_buffer.h"
#include "vulkan/compute_pipeline.h"

struct Vertex {
    glm::vec4 position;
    glm::vec4 normal;
    glm::vec4 color;
    glm::vec2 uv;
    glm::vec4 tangent;
};

float clear_col[4] = {0, 0, 0, 1};


// GraphicsPipeline get_equirect_to_cubemap_pipeline() {
//     VulkanVertexLayout vertex_layout;
//     LayoutInitializer layout_initializer = vertex_layout.get_initializer();
//     layout_initializer.add(AttrFormat::FLOAT4);
//     layout_initializer.add(AttrFormat::FLOAT4);
//     layout_initializer.add(AttrFormat::FLOAT4);
//     layout_initializer.add(AttrFormat::FLOAT2);
//     layout_initializer.add(AttrFormat::FLOAT4);
//     VkPipelineVertexInputStateCreateInfo vertexInput = vertex_layout.get_vertex_intput();

//     uniform_buffer = VideoBuffer(engine, sizeof(PBRUniform));

//     DescriptorSetBundleBuilder builder = DescriptorSetBundleBuilder();
//     builder.add_uniform_buffer(0, uniform_buffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
//     builder.add_combined_image_sampler(1, VK_SHADER_STAGE_FRAGMENT_BIT); // albedo

//     descriptor_set_bundle = builder.create(engine.device);

//     pipeline = GraphicsPipeline(engine, sizeof(PBRUniform), descriptor_set_bundle, vertex_layout, vertex_shader, fragment_shader);
// }

struct CubePosVertex {
    glm::vec4 position;
};

Mesh create_position_cube_mesh(VulkanEngine& engine) {
    std::vector<CubePosVertex> vertices = {
        // +Z
        { glm::vec4(-0.5f, -0.5f,  0.5f, 1.0f) },
        { glm::vec4( 0.5f, -0.5f,  0.5f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f,  0.5f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f,  0.5f, 1.0f) },

        // -Z
        { glm::vec4( 0.5f, -0.5f, -0.5f, 1.0f) },
        { glm::vec4(-0.5f, -0.5f, -0.5f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f, -0.5f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f, -0.5f, 1.0f) },

        // +X
        { glm::vec4( 0.5f, -0.5f,  0.5f, 1.0f) },
        { glm::vec4( 0.5f, -0.5f, -0.5f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f, -0.5f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f,  0.5f, 1.0f) },

        // -X
        { glm::vec4(-0.5f, -0.5f, -0.5f, 1.0f) },
        { glm::vec4(-0.5f, -0.5f,  0.5f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f,  0.5f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f, -0.5f, 1.0f) },

        // +Y
        { glm::vec4(-0.5f,  0.5f,  0.5f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f,  0.5f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f, -0.5f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f, -0.5f, 1.0f) },

        // -Y
        { glm::vec4(-0.5f, -0.5f, -0.5f, 1.0f) },
        { glm::vec4( 0.5f, -0.5f, -0.5f, 1.0f) },
        { glm::vec4( 0.5f, -0.5f,  0.5f, 1.0f) },
        { glm::vec4(-0.5f, -0.5f,  0.5f, 1.0f) },
    };

    std::vector<uint32_t> indices = {
         0,  1,  2,   2,  3,  0,   // +Z
         4,  5,  6,   6,  7,  4,   // -Z
         8,  9, 10,  10, 11,  8,   // +X
        12, 13, 14,  14, 15, 12,   // -X
        16, 17, 18,  18, 19, 16,   // +Y
        20, 21, 22,  22, 23, 20    // -Y
    };

    return Mesh(
        engine,
        vertices.data(),
        sizeof(CubePosVertex) * vertices.size(),
        indices.data(),
        sizeof(uint32_t) * indices.size()
    );
}

struct TempUniform {
    glm::vec4 input_val;
};


int main() {
    VulkanEngine engine = VulkanEngine();
    VulkanWindow window = VulkanWindow(&engine, 1280, 720, "3D visualization");
    engine.set_vulkan_window(&window);
    ui::init(window.window);

    Camera camera = Camera();
    window.set_camera(&camera);

    FPSCameraController camera_controller = FPSCameraController(&camera);
    camera_controller.speed = 20;

    ShaderModule vert_module = ShaderModule(engine.device, "shaders/pbr.vert.spv");
    ShaderModule frag_module = ShaderModule(engine.device, "shaders/pbr.frag.spv");

    ShaderModule equirect_to_cubemap_vs = ShaderModule(engine.device, "shaders/equirect_to_cubemap.vert.spv");
    ShaderModule equirect_to_cubemap_fs = ShaderModule(engine.device, "shaders/equirect_to_cubemap.frag.spv");

    PBRRenderer renderer = PBRRenderer(engine, vert_module, frag_module);
    
    std::vector<Vertex> vertices = {
        // +Z (front)
        { glm::vec4(-0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },

        // -Z (back)
        { glm::vec4( 0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4(-0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 1.0f) },

        // +X (right)
        { glm::vec4( 0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f), glm::vec2(0.0f, 0.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 1.0f) },
        { glm::vec4( 0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f), glm::vec2(1.0f, 0.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f), glm::vec2(0.0f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 1.0f) },

        // -X (left)
        { glm::vec4(-0.5f, -0.5f, -0.5f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 0.0f), glm::vec4(1.0f, 1.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 1.0f) },
        { glm::vec4(-0.5f, -0.5f,  0.5f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 0.0f), glm::vec4(1.0f, 1.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f,  0.5f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 0.0f), glm::vec4(1.0f, 1.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f, -0.5f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 0.0f), glm::vec4(1.0f, 1.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 1.0f) },

        // +Y (top)
        { glm::vec4(-0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  1.0f,  0.0f, 0.0f), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f), glm::vec2(0.0f, 0.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  1.0f,  0.0f, 0.0f), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f), glm::vec2(1.0f, 0.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  1.0f,  0.0f, 0.0f), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  1.0f,  0.0f, 0.0f), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f), glm::vec2(0.0f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },

        // -Y (bottom)
        { glm::vec4(-0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 0.0f, -1.0f,  0.0f, 0.0f), glm::vec4(0.0f, 1.0f, 1.0f, 1.0f), glm::vec2(0.0f, 0.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 0.0f, -1.0f,  0.0f, 0.0f), glm::vec4(0.0f, 1.0f, 1.0f, 1.0f), glm::vec2(1.0f, 0.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 0.0f, -1.0f,  0.0f, 0.0f), glm::vec4(0.0f, 1.0f, 1.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4(-0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 0.0f, -1.0f,  0.0f, 0.0f), glm::vec4(0.0f, 1.0f, 1.0f, 1.0f), glm::vec2(0.0f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
    };

    std::vector<uint32_t> indices = {
        // +Z (front)
        0, 1, 2,
        2, 3, 0,

        // -Z (back)
        4, 5, 6,
        6, 7, 4,

        // +X (right)
        8, 9, 10,
        10, 11, 8,

        // -X (left)
        12, 13, 14,
        14, 15, 12,

        // +Y (top)
        16, 17, 18,
        18, 19, 16,

        // -Y (bottom)
        20, 21, 22,
        22, 23, 20
    };

    Mesh mesh = Mesh(engine, vertices.data(), sizeof(Vertex) * vertices.size(), indices.data(), sizeof(uint32_t) * indices.size());

    // Texture2D hdr_texture = Texture2D(engine, "assets/hdr/st_peters_square_night_4k.hdr",
    //                 Texture2D::Wrap::Repeat,
    //                 Texture2D::MagFilter::Linear,
    //                 Texture2D::MinFilter::LinearMipmapLinear,
    //                 true,   // sRGB
    //                 true    // flipY
    // );
    // renderer.descriptor_set_bundle.bind_combined_image_sampler(1, hdr_texture);
    
    Texture2D storage_texture(engine, 10, 10, nullptr);
    storage_texture.transition_to_general_layout(engine);

    

    CommandPool command_pool(engine.device, engine.physicalDevice);
    CommandBuffer command_buffer(command_pool);

    ShaderModule compute_shader(engine.device, "shaders/test_compute.comp.spv");

    VideoBuffer temp_uniform_buffer(engine, sizeof(TempUniform));
    VideoBuffer temp_storage_buffer(engine, sizeof(glm::dvec4));

    DescriptorSetBundleBuilder builder = DescriptorSetBundleBuilder();
    builder.add_uniform_buffer(0, temp_uniform_buffer, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.add_storage_buffer(1, temp_storage_buffer, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.add_combined_image_sampler(2, storage_texture, VK_SHADER_STAGE_COMPUTE_BIT);
    DescriptorSetBundle descriptor_set_bundle = builder.create(engine.device);

    ComputePipeline compute_pipeline(engine.device, descriptor_set_bundle, compute_shader);

    command_buffer.begin();
    command_buffer.bind_pipeline(compute_pipeline);
    command_buffer.dispatch(1, 1, 1);
    command_buffer.memory_barrier(temp_storage_buffer);
    command_buffer.end();

    Fence fence(engine.device);

    command_buffer.submit(fence);

    fence.wait_for_fence();

    glm::dvec4 out{};
    temp_storage_buffer.read_subdata(0, &out, sizeof(glm::dvec4));

    std::cout << "(" << out.x << ", " << out.y << ", " << out.z << ", " << out.w << ")" << std::endl;

    storage_texture.transition_to_shader_read_only_layout(engine);

    renderer.descriptor_set_bundle.bind_combined_image_sampler(1, storage_texture);

    // Mesh cube_mesh = create_position_cube_mesh(engine);
    // Cubemap output_cubemap;

    // EquirectToCubemapPass equirect_to_cubemap_pass;
    // equirect_to_cubemap_pass.create(engine, 512);
    // equirect_to_cubemap_pass.render(cube_mesh, hdr_texture, output_cubemap);



    // renderer.descriptor_set_bundle.bind_combined_image_sampler(1, hdr_texture);

    // RenderPass offscreen_render_pass = RenderPassBuilder()
    //             .set_color_attachment(RenderPassBuilder::make_offscreen_color_attachment(VK_FORMAT_R16G16B16A16_SFLOAT))
    //             .set_depth_attachment(RenderPassBuilder::make_offscreen_depth_attachment(vulkan_utils::find_depth_format(engine.physicalDevice)))
    //             .create(engine.device);


    // RenderTarget2D target;
    // target.create_color_and_depth(
    //     engine,
    //     offscreen_render_pass,
    //     512,
    //     512,
    //     VK_FORMAT_R16G16B16A16_SFLOAT
    // );
    


    float last_frame = 0.0f;
    while(window.is_open()) {
        float currentFrame = (float)glfwGetTime();
        float delta_time = currentFrame - last_frame;
        last_frame = currentFrame;  

        camera_controller.update(&window, delta_time);

        engine.begin_frame(glm::vec4(0.01f, 0.01f, 0.01f, 1.0f));

        renderer.render(mesh, camera);

        engine.end_frame();
        engine.poll_events();
    }
}
