#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <chrono>
#include <cmath>
#include "vulkan_engine.h"
#include <array>
#include <tuple>
#include "camera.h"
#include "fps_camera_controller.h"

#include <cstdint>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <random>
#include "vulkan_window.h"
#include "vulkan/shader_module.h"
#include "vulkan/video_buffer.h"
#include "vulkan/vulkan_vertex_layout.h"
#include "vulkan/graphics_pipeline.h"
#include "vulkan/pbr_renderer.h"
#include "mesh.h"
#include "pbr_uniform.h"
#include "vulkan/image/texture2d.h"
#include "vulkan/render_target_2d.h"
#include "vulkan/render_pass.h"
#include "vulkan/image/cubemap.h"
#include "vulkan/command_pool.h"
#include "vulkan/command_buffer.h"
#include "vulkan/compute_pipeline.h"
#include "vulkan/image/cubemap.h"
#include "math_utils.h"
#include "vulkan/pbr/equirect_to_cubemap_pass.h"

#include "vulkan/resource_loader.h"
#include "skybox/skybox_pass.h"
#include "vulkan/pbr/brdf_lut_generator.h"
#include "vulkan/pbr/irradiance_map_generator.h"
#include "vulkan/pbr/prefilter_map_generator.h"
#include "vulkan/lighting_system/lighting_system.h"
#include "path_utils.h"

struct Vertex {
    glm::vec4 position;
    glm::vec4 normal;
    glm::vec4 color;
    glm::vec2 uv;
    glm::vec4 tangent;
};

float clear_col[4] = {0, 0, 0, 1};

struct CubePosVertex {
    glm::vec4 position;
};

struct EquirectToCubemapUniform {
    uint32_t image_width;
    uint32_t image_height;
    uint32_t num_layers;
};

Mesh create_cube_mesh(VulkanEngine& engine, glm::vec4 color) {
    std::vector<Vertex> vertices = {
        // +Z (front)
        { glm::vec4(-0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 0.0f), color, glm::vec2(0.0f, 0.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 0.0f), color, glm::vec2(1.0f, 0.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 0.0f), color, glm::vec2(1.0f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 0.0f), color, glm::vec2(0.0f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },

        // -Z (back)
        { glm::vec4( 0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 0.0f), color, glm::vec2(0.0f, 0.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4(-0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 0.0f), color, glm::vec2(1.0f, 0.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 0.0f), color, glm::vec2(1.0f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 0.0f), color, glm::vec2(0.0f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 1.0f) },

        // +X (right)
        { glm::vec4( 0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 0.0f), color, glm::vec2(0.0f, 0.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 1.0f) },
        { glm::vec4( 0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 0.0f), color, glm::vec2(1.0f, 0.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 0.0f), color, glm::vec2(1.0f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 0.0f), color, glm::vec2(0.0f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 1.0f) },

        // -X (left)
        { glm::vec4(-0.5f, -0.5f, -0.5f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 0.0f), color, glm::vec2(0.0f, 0.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 1.0f) },
        { glm::vec4(-0.5f, -0.5f,  0.5f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 0.0f), color, glm::vec2(1.0f, 0.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f,  0.5f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 0.0f), color, glm::vec2(1.0f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f, -0.5f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 0.0f), color, glm::vec2(0.0f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 1.0f) },

        // +Y (top)
        { glm::vec4(-0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  1.0f,  0.0f, 0.0f), color, glm::vec2(0.0f, 0.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  1.0f,  0.0f, 0.0f), color, glm::vec2(1.0f, 0.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  1.0f,  0.0f, 0.0f), color, glm::vec2(1.0f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  1.0f,  0.0f, 0.0f), color, glm::vec2(0.0f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },

        // -Y (bottom)
        { glm::vec4(-0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 0.0f, -1.0f,  0.0f, 0.0f), color, glm::vec2(0.0f, 0.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 0.0f, -1.0f,  0.0f, 0.0f), color, glm::vec2(1.0f, 0.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 0.0f, -1.0f,  0.0f, 0.0f), color, glm::vec2(1.0f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4(-0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 0.0f, -1.0f,  0.0f, 0.0f), color, glm::vec2(0.0f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
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

    Mesh mesh(engine,
              vertices.data(),
              sizeof(Vertex) * vertices.size(),
              indices.data(),
              sizeof(uint32_t) * indices.size());

    return mesh;
}

Mesh create_sphere_mesh(VulkanEngine& engine, glm::vec4 color) {
    constexpr float radius = 0.5f;
    constexpr uint32_t stacks = 32;
    constexpr uint32_t sectors = 64;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    vertices.reserve((stacks + 1) * (sectors + 1));
    indices.reserve(stacks * sectors * 6);

    const float pi = 3.14159265358979323846f;
    const float two_pi = 2.0f * pi;

    for (uint32_t i = 0; i <= stacks; ++i) {
        float v = static_cast<float>(i) / static_cast<float>(stacks);
        float phi = v * pi;

        float sin_phi = std::sin(phi);
        float cos_phi = std::cos(phi);

        for (uint32_t j = 0; j <= sectors; ++j) {
            float u = static_cast<float>(j) / static_cast<float>(sectors);
            float theta = u * two_pi;

            float sin_theta = std::sin(theta);
            float cos_theta = std::cos(theta);

            glm::vec3 normal(
                sin_phi * cos_theta,
                cos_phi,
                sin_phi * sin_theta
            );

            glm::vec3 position = radius * normal;

            glm::vec3 tangent;
            if (sin_phi > 1e-6f) {
                tangent = glm::normalize(glm::vec3(
                    -sin_theta,
                    0.0f,
                    cos_theta
                ));
            } else {
                tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            }

            vertices.push_back({
                glm::vec4(position, 1.0f),
                glm::vec4(glm::normalize(normal), 0.0f),
                color,
                glm::vec2(u, v),
                glm::vec4(tangent, 1.0f)
            });
        }
    }

    for (uint32_t i = 0; i < stacks; ++i) {
        for (uint32_t j = 0; j < sectors; ++j) {
            uint32_t k1 = i * (sectors + 1) + j;
            uint32_t k2 = k1 + sectors + 1;

            if (i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }

            if (i != stacks - 1) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }

    Mesh mesh(
        engine,
        vertices.data(),
        sizeof(Vertex) * vertices.size(),
        indices.data(),
        sizeof(uint32_t) * indices.size()
    );

    return mesh;
}


int main() {
    VulkanEngine engine;
    VulkanWindow window(&engine, 1280, 720, "3D visualization");
    engine.set_vulkan_window(&window);

    // ui::VulkanInitInfo ui_info{};
    // ui_info.instance = engine.instance;
    // ui_info.physical_device = engine.physicalDevice;
    // ui_info.device = engine.device;
    // ui_info.queue_family = engine.get_graphics_queue_family();
    // ui_info.queue = engine.graphicsQueue;
    // ui_info.descriptor_pool = engine.get_imgui_descriptor_pool();
    // ui_info.render_pass = engine.renderPass;
    // ui_info.min_image_count = engine.get_imgui_min_image_count();
    // ui_info.image_count = static_cast<uint32_t>(engine.swapchainImages.size());
    // ui_info.msaa_samples = VK_SAMPLE_COUNT_1_BIT;
    // ui_info.pipeline_cache = VK_NULL_HANDLE;
    // ui_info.subpass = 0;
    // ui_info.check_vk_result_fn = VulkanEngine::imgui_check_vk_result;

    // ui::init(window.window, ui_info);

    Camera camera = Camera();
    window.set_camera(&camera);

    FPSCameraController camera_controller = FPSCameraController(&camera);
    camera_controller.speed = 20;

    ShaderModule vert_module = ShaderModule(engine.device, executable_dir() / "shaders" / "pbr.vert.spv");
    ShaderModule frag_module = ShaderModule(engine.device, executable_dir() / "shaders" / "pbr.frag.spv");

    ShaderModule equirect_to_cubemap_vs = ShaderModule(engine.device, executable_dir() / "shaders" / "equirect_to_cubemap.vert.spv");
    ShaderModule equirect_to_cubemap_fs = ShaderModule(engine.device, executable_dir() / "shaders" / "equirect_to_cubemap.frag.spv");

    PBRRenderer renderer = PBRRenderer(engine, vert_module, frag_module, 0.3f);

    Mesh mesh = create_sphere_mesh(engine, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    Mesh sphere_2 = create_sphere_mesh(engine, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

    sphere_2.position.x = 2;
    std::vector<Mesh> spheres;

    for (int x = -5; x < 5; x++)
        for (int z = -5; z < 5; z++) {
            // Mesh sphere_mesh = create_sphere_mesh(engine, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

            // sphere_mesh.position = glm::vec3(x, 0, z);

            spheres.push_back(create_sphere_mesh(engine, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)));
            spheres.back().position = glm::vec3(x, 0, z);
        }


    EquirectToCubemapPass equirect_to_cubemap_pass(engine);
    BrdfLutGenerater brdf_lut_generator(engine);
    IrradianceMapGenerator irradiance_map_generator(engine);
    PrefilterMapGenerator prefilter_map_generator(engine);

    ResourceLoader resource_loader;
    resource_loader.create(engine, 154'217'728);

    // Texture2D equirectangular_map = resource_loader.load_hdr_texture2d("assets/hdr/st_peters_square_night_4k.hdr", 6, VK_IMAGE_USAGE_SAMPLED_BIT);
    // Texture2D equirectangular_map = resource_loader.load_hdr_texture2d(executable_dir() / "assets" / "hdr" / "citrus_orchard_puresky_4k.hdr", 6, VK_IMAGE_USAGE_SAMPLED_BIT);
    // Texture2D equirectangular_map = resource_loader.load_hdr_texture2d(executable_dir() / "assets" / "hdr" / "studio_kominka_02_4k.hdr", 6, VK_IMAGE_USAGE_SAMPLED_BIT);

    // Mesh mesh = create_cube_mesh(engine, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));

    Texture2D driveaway_texture = resource_loader.load_texture2d(executable_dir() / "assets" / "textures" / "driveaway" / "diff.png", 1, VK_IMAGE_USAGE_SAMPLED_BIT);

    std::cout << "=== image_resource ===" << std::endl;
    std::cout << "image: " << (driveaway_texture.image_resource.image == VK_NULL_HANDLE ? "VK_NULL_HANDLE" : "true") << std::endl;
    std::cout << "memory: " << (driveaway_texture.image_resource.memory == VK_NULL_HANDLE ? "VK_NULL_HANDLE" : "true") << std::endl;
    std::cout << std::endl;

    std::cout << "=== image_view ===" << std::endl;
    std::cout << "view: " << (driveaway_texture.image_view.view == VK_NULL_HANDLE ? "VK_NULL_HANDLE" : "true") << std::endl;
    std::cout << std::endl;

    std::cout << "=== sampler ===" << std::endl;
    std::cout << "sampler: " << (driveaway_texture.sampler.sampler == VK_NULL_HANDLE ? "VK_NULL_HANDLE" : "true") << std::endl;
    std::cout << std::endl;
    
    
    
    
    // Texture2D brdf_lut = brdf_lut_generator.generate(256, 256);
    
    // Cubemap environment_map = equirect_to_cubemap_pass.generate(equirectangular_map, 1920);
    // Cubemap irradiance_map = irradiance_map_generator.generate(environment_map, 256);
    // Cubemap prefilter_map = prefilter_map_generator.generate(environment_map, 256);

    // SkyboxPass skybox_pass;
    // skybox_pass.create(engine);

    LightingSystem lighting_system;
    lighting_system.init(engine);

    LightSource red_light_source{glm::vec4(1, 2, 0, 10), glm::vec4(1, 0, 0, 1)};
    LightSource green_light_source{glm::vec4(0, 2, 1, 10), glm::vec4(0, 1, 0, 1)};
    LightSource blue_light_source{glm::vec4(-1, 2, 0, 10), glm::vec4(0, 0, 1, 1)};

    // light_source.position = glm::vec4(0, 2, 0, 10);
    // light_source.color = glm::vec4(1, 0, 0, 1);

    lighting_system.set_light_source(0, red_light_source);
    lighting_system.set_light_source(1, green_light_source);
    lighting_system.set_light_source(2, blue_light_source);

    // lighting_system.set_light_source(0, light_source);
    float timer = 0.0f;
    float last_frame = 0.0f;
    while(window.is_open()) {
        float currentFrame = (float)glfwGetTime();
        float delta_time = currentFrame - last_frame;
        last_frame = currentFrame;  

        camera_controller.update(&window, delta_time);

        red_light_source.position.y = sin(timer * 2 + 0.234f) * 2;
        green_light_source.position.x = sin(timer * 2 + 1.3423f) * 2;
        blue_light_source.position.z = cos(timer * 2) * 2;

        lighting_system.set_light_source(0, red_light_source);
        lighting_system.set_light_source(1, green_light_source);
        lighting_system.set_light_source(2, blue_light_source);
        
        // engine.begin_frame(glm::vec4(0.01f, 0.01f, 0.01f, 1.0f));
        engine.begin_frame(glm::vec4(132 / 255.0f, 189 / 255.0f, 250 / 255.0f, 1.0f));
        // lighting_system.update(camera);

        // skybox_pass.render(camera, environment_map);
        // renderer.render(sphere_2, camera, irradiance_map, prefilter_map, brdf_lut, lighting_system);
        // renderer.render(mesh, camera, irradiance_map, prefilter_map, brdf_lut, lighting_system);
        
        for (int i = 0; i < spheres.size(); i++)
            renderer.render(spheres[i], camera, driveaway_texture, lighting_system);

        engine.end_frame();
        engine.poll_events();

        timer += delta_time;
    }
}
