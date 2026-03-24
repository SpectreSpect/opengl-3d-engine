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
#include "vulkan/cubemap.h"
#include "math_utils.h"


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

struct EquirectToCubemapUniform {
    uint32_t image_width;
    uint32_t image_height;
    uint32_t num_layers;
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

    Texture2D hdr_texture = Texture2D(engine, "assets/hdr/st_peters_square_night_4k.hdr",
                    Texture2D::Wrap::Repeat,
                    Texture2D::MagFilter::Linear,
                    Texture2D::MinFilter::LinearMipmapLinear,
                    true,   // sRGB
                    true    // flipY
    );
    renderer.descriptor_set_bundle.bind_combined_image_sampler(1, hdr_texture);

    int face_size = 512;
    
    // std::array<Texture2D, 6> faces;

    // for (int i = 0; i < faces.size(); i++) {
    //     faces[i].create(engine, face_size, face_size, nullptr);
    //     faces[i].transition_to_general_layout(engine);
    // }

    // std::array<Texture2D, 6> faces;

    // Texture2D equirectangular_map(engine, "assets/hdr/st_peters_square_night_4k.hdr");

    Texture2D equirectangular_map = Texture2D(engine, "assets/hdr/st_peters_square_night_4k.hdr",
                Texture2D::Wrap::Repeat,
                Texture2D::MagFilter::Linear,
                Texture2D::MinFilter::LinearMipmapLinear,
                true,   // sRGB
                true    // flipY
    );
    // equirectangular_map.transition_to_general_layout(engine);

    // std::array<std::string, 6> paths = {
    //     "assets/hdr/st_peters_square_night_4k.hdr",
    //     "assets/hdr/st_peters_square_night_4k.hdr",
    //     "assets/hdr/st_peters_square_night_4k.hdr",
    //     "assets/hdr/st_peters_square_night_4k.hdr",
    //     "assets/hdr/st_peters_square_night_4k.hdr",
    //     "assets/hdr/st_peters_square_night_4k.hdr"
    // };

    // std::array<std::string, 6> paths = {
    //     "assets/textures/minecraft_dirt/texture.png",
    //     "assets/textures/minecraft_dirt/texture.png",
    //     "assets/textures/minecraft_dirt/texture.png",
    //     "assets/textures/minecraft_dirt/texture.png",
    //     "assets/textures/minecraft_dirt/texture.png",
    //     "assets/textures/minecraft_dirt/texture.png"
    // };

    // Cubemap cubemap(engine, paths);
    Cubemap cubemap;
    cubemap.createEmpty(engine, face_size);
    cubemap.transition_to_general_layout(engine);
    // cubemap.transition_image_layout();
    

    CommandPool command_pool(engine.device, engine.physicalDevice);
    CommandBuffer command_buffer(command_pool);

    ShaderModule equirect_to_cubemap_cs(engine.device, "shaders/equirect_to_cubemap.comp.spv");

    VideoBuffer equirect_to_cubemap_uniform_buffer(engine, sizeof(EquirectToCubemapUniform));
    // VideoBuffer temp_storage_buffer(engine, sizeof(glm::dvec4));

    EquirectToCubemapUniform equirect_to_cubemap_uniform{};
    equirect_to_cubemap_uniform.image_width = face_size;
    equirect_to_cubemap_uniform.image_height = face_size;
    equirect_to_cubemap_uniform.num_layers = 6;
    equirect_to_cubemap_uniform_buffer.update_data(&equirect_to_cubemap_uniform, sizeof(EquirectToCubemapUniform));


    DescriptorSetBundleBuilder builder = DescriptorSetBundleBuilder();
    builder.add_uniform_buffer(0, equirect_to_cubemap_uniform_buffer, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.add_combined_image_sampler(1, equirectangular_map, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.add_image_storage(2, cubemap, VK_SHADER_STAGE_COMPUTE_BIT);
    
    DescriptorSetBundle descriptor_set_bundle = builder.create(engine.device);

    ComputePipeline compute_pipeline(engine.device, descriptor_set_bundle, equirect_to_cubemap_cs);

    uint32_t x_groups = math_utils::div_up_u32(equirect_to_cubemap_uniform.image_width, 256);
    uint32_t y_groups = equirect_to_cubemap_uniform.image_height;
    uint32_t z_groups = equirect_to_cubemap_uniform.num_layers;
    

    command_buffer.begin();
    command_buffer.bind_pipeline(compute_pipeline);
    command_buffer.dispatch(x_groups, y_groups, z_groups);
    // command_buffer.memory_barrier(temp_storage_buffer);
    command_buffer.end();

    Fence fence(engine.device);

    command_buffer.submit(fence);

    fence.wait_for_fence();

    // glm::dvec4 out{};
    // temp_storage_buffer.read_subdata(0, &out, sizeof(glm::dvec4));

    // std::cout << "(" << out.x << ", " << out.y << ", " << out.z << ", " << out.w << ")" << std::endl;

    // equirectangular_map.transition_to_shader_read_only_layout(engine);
    cubemap.transition_to_shader_read_only_layout(engine);

    // renderer.descriptor_set_bundle.bind_combined_image_sampler(1, faces[0]);
    renderer.descriptor_set_bundle.bind_combined_image_sampler(2, cubemap);

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
