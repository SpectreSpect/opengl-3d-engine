// main.cpp
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <chrono>
#include <cmath>

#include "engine3d.h"

#include "vao.h"
#include "vbo.h"
#include "ebo.h"
#include "vertex_layout.h"
#include "program.h"
#include "camera.h"
#include "mesh.h"
#include "cube.h"
#include "window.h"
#include "fps_camera_controller.h"
#include "voxel_engine/chunk.h"
#include "voxel_engine/voxel_grid.h"
#include "imgui_layer.h"
#include "voxel_rastorizator.h"
#include "ui_elements/triangle_controller.h"
#include "triangle.h"
#include "a_star/a_star.h"
#include "line.h"
#include "a_star/nonholonomic_a_star.h"
#include "a_star/reeds_shepp.h"

#include <cstdint>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include "point.h"
#include "point_cloud/point_cloud_frame.h"
#include "point_cloud/point_cloud_video.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>
#include <cmath>
#include <cstdint>
#include "math_utils.h"
#include "light_source.h"
#include "circle_cloud.h"
#include "texture.h"
#include "cubemap.h"
#include "skybox.h"
#include "framebuffer.h"

float clear_col[4] = {0.15f, 0.15f, 0.18f, 1.0f};

void set_light_position(SSBO& light_source_ssbo, size_t index, const glm::vec4 &pos) {
    size_t offset = index * sizeof(LightSource);
    light_source_ssbo.update_subdata(offset, &pos, sizeof(pos));
}

Cubemap generateIrradianceMap(Engine3D& engine, Mesh* cube_mesh, Cubemap& envMap, std::array<glm::mat4, 6> captureViews, glm::mat4 captureProj) {
    Cubemap irradianceMap;
    int irradianceSize = 32;

    irradianceMap.createEmpty(
        irradianceSize,
        GL_RGB16F,
        Cubemap::MagFilter::Linear,
        Cubemap::MinFilter::Linear
    );

    Framebuffer irradianceFBO(irradianceSize, irradianceSize);
    irradianceFBO.attachCubemapFace(irradianceMap.id, 0, GL_COLOR_ATTACHMENT0, 0);
    irradianceFBO.createDepthRenderbuffer(GL_DEPTH_COMPONENT24);
    irradianceFBO.setDrawBuffer(GL_COLOR_ATTACHMENT0);

    if (!irradianceFBO.isComplete()) {
        std::cerr << "Irradiance FBO incomplete\n";
    }

    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);

    irradianceFBO.bind();
    glViewport(0, 0, irradianceSize, irradianceSize);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    engine.irradiance_program->use();
    engine.irradiance_program->set_mat4("uProj", captureProj);
    engine.irradiance_program->set_int("uEnvironmentMap", 0);
    envMap.bind(0);

    for (int face = 0; face < 6; ++face) {
        irradianceFBO.attachCubemapFace(irradianceMap.id, face, GL_COLOR_ATTACHMENT0, 0);
        engine.irradiance_program->set_mat4("uView", captureViews[face]);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        cube_mesh->vao->bind();
        glDrawElements(GL_TRIANGLES, cube_mesh->ebo->num_indices, GL_UNSIGNED_INT, 0);
        cube_mesh->vao->unbind();
    }

    Framebuffer::unbind();
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);


    return irradianceMap;
}

Cubemap generatePrefilterMap(Engine3D& engine, Mesh* cube_mesh, Cubemap& envMap, std::array<glm::mat4, 6> captureViews, glm::mat4 captureProj) {
    Cubemap prefilterMap;
    int prefilterSize = 128;

    prefilterMap.createEmpty(
        prefilterSize,
        GL_RGB16F,
        Cubemap::MagFilter::Linear,
        Cubemap::MinFilter::LinearMipmapLinear
    );

    Framebuffer prefilterFBO(prefilterSize, prefilterSize);
    prefilterFBO.attachCubemapFace(prefilterMap.id, 0, GL_COLOR_ATTACHMENT0, 0);
    prefilterFBO.createDepthRenderbuffer(GL_DEPTH_COMPONENT24);
    prefilterFBO.setDrawBuffer(GL_COLOR_ATTACHMENT0);

    if (!prefilterFBO.isComplete()) {
        std::cerr << "Prefilter FBO incomplete\n";
    }

    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);

    prefilterFBO.bind();
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    engine.prefilter_program->use();
    engine.prefilter_program->set_mat4("uProj", captureProj);
    engine.prefilter_program->set_int("uEnvironmentMap", 0);
    engine.prefilter_program->set_float("uEnvironmentResolution", float(envMap.faceSize()));
    envMap.bind(0);

    int maxMipLevels = prefilterMap.mipLevels();

    for (int mip = 0; mip < maxMipLevels; ++mip) {
        int mipWidth  = std::max(1, prefilterSize >> mip);
        int mipHeight = std::max(1, prefilterSize >> mip);

        glViewport(0, 0, mipWidth, mipHeight);

        // Depth RBO must match the current mip size
        glNamedRenderbufferStorage(
            prefilterFBO.depth_rbo,
            GL_DEPTH_COMPONENT24,
            mipWidth,
            mipHeight
        );

        float roughness = (maxMipLevels == 1)
            ? 0.0f
            : float(mip) / float(maxMipLevels - 1);

        engine.prefilter_program->use();
        engine.prefilter_program->set_float("uRoughness", roughness);

        for (int face = 0; face < 6; ++face) {
            prefilterFBO.attachCubemapFace(prefilterMap.id, face, GL_COLOR_ATTACHMENT0, mip);
            engine.prefilter_program->set_mat4("uView", captureViews[face]);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            cube_mesh->vao->bind();
            glDrawElements(GL_TRIANGLES, cube_mesh->ebo->num_indices, GL_UNSIGNED_INT, 0);
            cube_mesh->vao->unbind();
        }
    }

    Framebuffer::unbind();
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);

    return prefilterMap;
}

GLuint generateBrdfLut(Engine3D& engine, Mesh* quad_mesh, Cubemap& envMap, std::array<glm::mat4, 6> captureViews, glm::mat4 captureProj) {
    GLuint brdfLUT = 0;
    glCreateTextures(GL_TEXTURE_2D, 1, &brdfLUT);
    glTextureStorage2D(brdfLUT, 1, GL_RGBA16F, 512, 512);
    glTextureParameteri(brdfLUT, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(brdfLUT, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(brdfLUT, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(brdfLUT, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    Framebuffer brdfFBO(512, 512);
    brdfFBO.attachTexture2D(brdfLUT, GL_COLOR_ATTACHMENT0, 0);
    brdfFBO.setDrawBuffer(GL_COLOR_ATTACHMENT0);

    if (!brdfFBO.isComplete()) {
        std::cerr << "BRDF LUT FBO incomplete\n";
    }

    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);

    brdfFBO.bind();
    glViewport(0, 0, 512, 512);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    engine.brdf_lut_program->use();

    glClear(GL_COLOR_BUFFER_BIT);

    quad_mesh->vao->bind();
    glDrawElements(GL_TRIANGLES, quad_mesh->ebo->num_indices, GL_UNSIGNED_INT, 0);
    quad_mesh->vao->unbind();

    Framebuffer::unbind();
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
    glEnable(GL_DEPTH_TEST);

    return brdfLUT;
}


int main() {
    Engine3D engine = Engine3D();
    Window window = Window(&engine, 1280, 720, "3D visualization");
    engine.set_window(&window);
    ui::init(window.window);
    
    Camera camera;
    window.set_camera(&camera);
    FPSCameraController camera_controller = FPSCameraController(&camera);
    
    camera_controller.speed = 50;

    VoxelGrid* voxel_grid = new VoxelGrid({16, 16, 16}, 1.0f, {24, 10, 24});
    voxel_grid->update(&window, &camera);
    sleep(1);

    glm::vec3 origin = glm::vec3(0.0f, 10.0f, 0.0f);

    std::vector<Line*> point_cloud_video_lines;
    std::vector<Point*> point_cloud_video_points;
    std::vector<Mesh*> lidar_meshes;

    PointCloudFrame point_cloud_frame = PointCloudFrame("/home/spectre/TEMP_lidar_output_mesh/recording/frame_000000.bin");
    size_t rings_count = 16;
    size_t ring_size = point_cloud_frame.point_cloud.size() / rings_count;
    
    // Mesh point_cloud_mesh = point_cloud_frame.point_cloud.generate_mesh(1.5);

    

    Mesh point_cloud_mesh = point_cloud_frame.point_cloud.generate_mesh_gpu(rings_count, ring_size, 1.5);

    // point_cloud_mesh.position = glm::vec3(30, 5, 10);
    // point_cloud_mesh.scale = glm::vec3(5, 5, 5);

    // int size = 50;
    // for (int x = 0; x < size; x++) {
    //     for (int z = 0; z < size; z++) {
    //         int idx = z + x * size;
    //         float r = x / (float)size;
    //         float g = z / (float)size;
    //         glm::vec3 pos = glm::vec3(-x * 15, 10, -z * 15);
    //         voxel_grid->adjust_to_ground(pos);
    //         // engine.set_light_source(idx, {glm::vec4(pos.x, pos.y, pos.z, 10.0f), glm::vec4(r, g, 1.0f, 1.0f)});
    //         engine.lighting_system.set_light_source(idx, {glm::vec4(pos.x, pos.y + 5, pos.z, 30.0f), glm::vec4(0.9f, 0.9f, 1.0f, 1.0f)});    
    //     }
    // }

    // int size = 5;
    // for (int x = 0; x < size; x++) {
    //     engine.lighting_system.set_light_source(x, {glm::vec4(x * -50, 20, 0, 50.0f), glm::vec4(0.9f, 0.9f, 1.0f, 1.0f)});  
    // }

      

    // unsigned int num_clusters = engine.lighting_system.num_clusters.x * engine.lighting_system.num_clusters.y * engine.lighting_system.num_clusters.z;

    CirlceInstance circle_instance;
    circle_instance.color = glm::vec4(1, 1, 1, 1);

    std::vector<CirlceInstance> instances;

    int row_size = 8;
    for (int x = 0; x < row_size; x++)
        for (int z = 0; z < row_size; z++) {
            circle_instance.position_radius.x = (x + 0.5) * 2;
            circle_instance.position_radius.z = (z + 0.5) * 2;

            // circle_instance.color = glm::vec4((float)x / (float)row_size, 0, (float)z / (float)row_size, 1);

            instances.push_back(circle_instance);
        }
    
    // engine.lighting_system.set_light_source(0, {glm::vec4(((float)row_size / 2.0f) * 2.0f, 2, ((float)row_size / 2.0f) * 2.0f, 10.0f), glm::vec4(0.9f, 0.9f, 1.0f, 1.0f)});

    // instances.push_back(circle_instance);
    // circle_instance.position_radius.x += 5;
    // circle_instance.position_radius.w = 5;
    // circle_instance.normal = glm::vec4(1, 0, 0, 1);
    // circle_instance.color = glm::vec4(0, 0, 1, 1);
    // instances.push_back(circle_instance);

    CircleCloud circle_cloud = CircleCloud();
    circle_cloud.set_instances(instances);

    static std::vector<float> vertices = {
        // 0
        -1.0f, -1.0f, -1.0f,
        // 1
         1.0f, -1.0f, -1.0f,
        // 2
         1.0f,  1.0f, -1.0f,
        // 3
        -1.0f,  1.0f, -1.0f,
        // 4
        -1.0f, -1.0f,  1.0f,
        // 5
         1.0f, -1.0f,  1.0f,
        // 6
         1.0f,  1.0f,  1.0f,
        // 7
        -1.0f,  1.0f,  1.0f
    };

    static std::vector<unsigned int> indices = {
        // -Z
        0, 2, 1,
        0, 3, 2,

        // +Z
        4, 5, 6,
        4, 6, 7,

        // -X
        0, 7, 3,
        0, 4, 7,

        // +X
        1, 2, 6,
        1, 6, 5,

        // +Y
        3, 7, 6,
        3, 6, 2,

        // -Y
        0, 1, 5,
        0, 5, 4
    };

    VertexLayout* vertex_layout = new VertexLayout();
    vertex_layout->add("position", 0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0, 0, {0.0f, 0.0f, 0.0f});

    Mesh* cube_mesh = new Mesh(vertices, indices, vertex_layout);

    static std::vector<float> quadVertices = {
        -1.0f, -1.0f,
        1.0f, -1.0f,
        1.0f,  1.0f,
        -1.0f,  1.0f
    };

    static std::vector<unsigned int> quadIndices = {
        0, 1, 2,
        0, 2, 3
    };

    VertexLayout* quadLayout = new VertexLayout();
    quadLayout->add("position", 0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0, 0, {0.0f, 0.0f});

    Mesh* quad_mesh = new Mesh(quadVertices, quadIndices, quadLayout);


    Texture hdrEquirect;
    hdrEquirect.loadFromFile(
        "assets/hdr/citrus_orchard_puresky_4k.hdr",
        // "assets/hdr/ferndale_studio_06_4k.hdr",
        Texture::Wrap::ClampToEdge,
        Texture::MagFilter::Linear,
        Texture::MinFilter::Linear,
        false,
        true
    );


    

    Cubemap envMap;
    // bool ok = envMap.loadFromFaces({
    //     (executable_dir() / "assets" / "textures" / "minecraft_dirt" / "texture.png").string(),
    //     (executable_dir() / "assets" / "textures" / "minecraft_dirt" / "texture.png").string(),
    //     (executable_dir() / "assets" / "textures" / "minecraft_dirt" / "texture.png").string(),
    //     (executable_dir() / "assets" / "textures" / "minecraft_dirt" / "texture.png").string(),
    //     (executable_dir() / "assets" / "textures" / "minecraft_dirt" / "texture.png").string(),
    //     (executable_dir() / "assets" / "textures" / "minecraft_dirt" / "texture.png").string(),
    // });
    

    int size = 4096;
    envMap.createEmpty(
        size,
        GL_RGB16F,
        Cubemap::MagFilter::Linear,
        Cubemap::MinFilter::LinearMipmapLinear
    );

    Framebuffer capture_fbo(size, size);
    capture_fbo.attachCubemapFace(envMap.id, 0, GL_COLOR_ATTACHMENT0, 0);
    capture_fbo.createDepthRenderbuffer(GL_DEPTH_COMPONENT24);
    capture_fbo.setDrawBuffer(GL_COLOR_ATTACHMENT0);

    if (!capture_fbo.isComplete()) {
        // handle error
    }


    glm::mat4 captureProj = glm::perspective(
        glm::radians(90.0f), 1.0f, 0.1f, 10.0f
    );

    std::array<glm::mat4, 6> captureViews = {
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // +X
        glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // -X
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)), // +Y
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)), // -Y
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // +Z
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))  // -Z
    };

    // equirect_to_cubemap_program

    engine.equirect_to_cubemap_program->use();
    engine.equirect_to_cubemap_program->set_mat4("uProj", captureProj);
    engine.equirect_to_cubemap_program->set_int("uEquirectangularMap", 0);
    hdrEquirect.bind(0);

    capture_fbo.bind();
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);

    for (int face = 0; face < 6; ++face) {
        capture_fbo.attachCubemapFace(envMap.id, face, GL_COLOR_ATTACHMENT0, 0);

        engine.equirect_to_cubemap_program->use();
        engine.equirect_to_cubemap_program->set_mat4("uView", captureViews[face]);

        glViewport(0, 0, size, size);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        cube_mesh->vao->bind();
        glDrawElements(GL_TRIANGLES, cube_mesh->ebo->num_indices, GL_UNSIGNED_INT, 0);
        cube_mesh->vao->unbind();
    }

    Framebuffer::unbind();
    glGenerateTextureMipmap(envMap.id);

    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);

    Cubemap irradiance_map = generateIrradianceMap(engine, cube_mesh, envMap, captureViews, captureProj);
    Cubemap prefilter_map = generatePrefilterMap(engine, cube_mesh, envMap, captureViews, captureProj);
    GLuint brdf_lut = generateBrdfLut(engine, quad_mesh, envMap, captureViews, captureProj);


    #include <glm/glm.hpp>
    #include <vector>
    #include <cmath>
    #include <algorithm>

    constexpr float PI = 3.14159265358979323846f;

    float radius = 1.0f;
    int sectors = 64;
    int stacks  = 32;

    struct SphereBuildVertex {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec3 color;
        glm::vec2 uv;

        glm::vec3 tangent_accum   = glm::vec3(0.0f);
        glm::vec3 bitangent_accum = glm::vec3(0.0f);
    };

    std::vector<SphereBuildVertex> temp_vertices;
    std::vector<unsigned int> sphere_indices;

    temp_vertices.reserve((stacks + 1) * (sectors + 1));
    sphere_indices.reserve(stacks * sectors * 6);

    // ------------------------------------------------------------
    // Build sphere vertices
    // ------------------------------------------------------------
    for (int i = 0; i <= stacks; ++i) {
        float stackFrac  = float(i) / float(stacks);
        float stackAngle = PI * 0.5f - stackFrac * PI; // +pi/2 -> -pi/2

        float xy = std::cos(stackAngle);
        float y  = std::sin(stackAngle);

        for (int j = 0; j <= sectors; ++j) {
            float sectorFrac  = float(j) / float(sectors);
            float sectorAngle = sectorFrac * 2.0f * PI;

            float x = xy * std::cos(sectorAngle);
            float z = xy * std::sin(sectorAngle);

            float uvScale = 3.0f;
            float u = sectorFrac * uvScale;
            float v = (1.0f - stackFrac) * uvScale;

            SphereBuildVertex vert;
            vert.pos    = glm::vec3(radius * x, radius * y, radius * z);
            vert.normal = glm::normalize(glm::vec3(x, y, z));
            vert.color  = glm::vec3(1.0f, 1.0f, 1.0f);
            vert.uv     = glm::vec2(u, v);

            temp_vertices.push_back(vert);
        }
    }

    // ------------------------------------------------------------
    // Build sphere indices
    // ------------------------------------------------------------
    for (int i = 0; i < stacks; ++i) {
        int k1 = i * (sectors + 1);
        int k2 = k1 + sectors + 1;

        for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) {
                sphere_indices.push_back(k1);
                sphere_indices.push_back(k2);
                sphere_indices.push_back(k1 + 1);
            }

            if (i != (stacks - 1)) {
                sphere_indices.push_back(k1 + 1);
                sphere_indices.push_back(k2);
                sphere_indices.push_back(k2 + 1);
            }
        }
    }

    // ------------------------------------------------------------
    // Accumulate tangents / bitangents per triangle
    // ------------------------------------------------------------
    for (size_t i = 0; i + 2 < sphere_indices.size(); i += 3) {
        unsigned int i0 = sphere_indices[i + 0];
        unsigned int i1 = sphere_indices[i + 1];
        unsigned int i2 = sphere_indices[i + 2];

        SphereBuildVertex& v0 = temp_vertices[i0];
        SphereBuildVertex& v1 = temp_vertices[i1];
        SphereBuildVertex& v2 = temp_vertices[i2];

        glm::vec3 p0 = v0.pos;
        glm::vec3 p1 = v1.pos;
        glm::vec3 p2 = v2.pos;

        glm::vec2 uv0 = v0.uv;
        glm::vec2 uv1 = v1.uv;
        glm::vec2 uv2 = v2.uv;

        glm::vec3 edge1 = p1 - p0;
        glm::vec3 edge2 = p2 - p0;

        glm::vec2 duv1 = uv1 - uv0;
        glm::vec2 duv2 = uv2 - uv0;

        float denom = duv1.x * duv2.y - duv2.x * duv1.y;
        if (std::abs(denom) < 1e-8f)
            continue;

        float f = 1.0f / denom;

        glm::vec3 tangent =
            f * (duv2.y * edge1 - duv1.y * edge2);

        glm::vec3 bitangent =
            f * (-duv2.x * edge1 + duv1.x * edge2);

        v0.tangent_accum   += tangent;
        v1.tangent_accum   += tangent;
        v2.tangent_accum   += tangent;

        v0.bitangent_accum += bitangent;
        v1.bitangent_accum += bitangent;
        v2.bitangent_accum += bitangent;
    }

    // ------------------------------------------------------------
    // Build final interleaved float array:
    // position(3), normal(3), color(3), uv(2), tangent(4)
    // ------------------------------------------------------------
    std::vector<float> sphere_vertices;
    sphere_vertices.reserve(temp_vertices.size() * 15);

    for (const SphereBuildVertex& v : temp_vertices) {
        glm::vec3 N = glm::normalize(v.normal);

        glm::vec3 T = v.tangent_accum;

        // Fallback if tangent is degenerate
        if (glm::length(T) < 1e-6f) {
            glm::vec3 ref = (std::abs(N.y) < 0.999f)
                ? glm::vec3(0.0f, 1.0f, 0.0f)
                : glm::vec3(1.0f, 0.0f, 0.0f);
            T = glm::normalize(glm::cross(ref, N));
        } else {
            // Gram-Schmidt orthogonalization
            T = glm::normalize(T - N * glm::dot(N, T));
        }

        glm::vec3 B = v.bitangent_accum;
        float handedness = (glm::dot(glm::cross(N, T), B) < 0.0f) ? -1.0f : 1.0f;

        // position
        sphere_vertices.push_back(v.pos.x);
        sphere_vertices.push_back(v.pos.y);
        sphere_vertices.push_back(v.pos.z);

        // normal
        sphere_vertices.push_back(N.x);
        sphere_vertices.push_back(N.y);
        sphere_vertices.push_back(N.z);

        // color
        sphere_vertices.push_back(v.color.x);
        sphere_vertices.push_back(v.color.y);
        sphere_vertices.push_back(v.color.z);

        // uv
        sphere_vertices.push_back(v.uv.x);
        sphere_vertices.push_back(v.uv.y);

        // tangent (xyz + handedness)
        sphere_vertices.push_back(T.x);
        sphere_vertices.push_back(T.y);
        sphere_vertices.push_back(T.z);
        sphere_vertices.push_back(handedness);
    }

    VertexLayout* sphere_vertex_layout = new VertexLayout();
    sphere_vertex_layout->add("position", 0, 3, GL_FLOAT, GL_FALSE, 15 * sizeof(float), 0, 0, {0.0f, 0.0f, 0.0f});
    sphere_vertex_layout->add("normal",   1, 3, GL_FLOAT, GL_FALSE, 15 * sizeof(float), 3 * sizeof(float), 0, {0.0f, 1.0f, 0.0f});
    sphere_vertex_layout->add("color",    2, 3, GL_FLOAT, GL_FALSE, 15 * sizeof(float), 6 * sizeof(float), 0, {1.0f, 1.0f, 1.0f});
    sphere_vertex_layout->add("uv",       3, 2, GL_FLOAT, GL_FALSE, 15 * sizeof(float), 9 * sizeof(float), 0, {0.0f, 0.0f});
    sphere_vertex_layout->add("tangent",  4, 4, GL_FLOAT, GL_FALSE, 15 * sizeof(float), 11 * sizeof(float), 0, {1.0f, 0.0f, 0.0f, 1.0f});

    Mesh* sphere_mesh = new Mesh(sphere_vertices, sphere_indices, sphere_vertex_layout);    

    // Texture* albedo = new Texture("assets/textures/slate/diffuse.png");
    // Texture* normal_map = new Texture("assets/textures/slate/normal.jpg");
    // Texture* orm_map = new Texture("assets/textures/slate/orm.jpg");

    Texture* albedo = new Texture("assets/textures/driveaway/diff.png");
    Texture* normal_map = new Texture("assets/textures/driveaway/normal.jpg");
    Texture* orm_map = new Texture("assets/textures/driveaway/arm.jpg");

    

    // Texture* albedo = new Texture("assets/textures/metal_plate/metal_plate_02_diff_1k.jpg");
    // Texture* normal_map = new Texture("assets/textures/metal_plate/metal_plate_02_nor_gl_1k.exr");
    // Texture* orm_map = new Texture("assets/textures/slate/orm.jpg");
    
    Skybox skybox = Skybox(envMap);
        
    float rel_thresh = 1.5f;
    float last_frame = 0.0f;
    float timer = 0.0f;
    // unsigned int counts[num_clusters];
    while(window.is_open()) {
        float currentFrame = (float)glfwGetTime();
        float delta_time = currentFrame - last_frame;
        timer += delta_time;
        last_frame = currentFrame;   
        float fps = 1.0f / delta_time;
        
        camera_controller.update(&window, delta_time);

        engine.update_lighting_system(camera, window);
        window.clear_color({clear_col[0], clear_col[1], clear_col[2], clear_col[3]});

        

        window.draw(&skybox, &camera);

        engine.default_program->use();
        irradiance_map.bind(0);
        engine.default_program->set_int("irradianceMap", 0);

        prefilter_map.bind(1);
        engine.default_program->set_int("prefilterMap", 1);
        engine.default_program->set_float("uPrefilterMaxMip", float(prefilter_map.mipLevels() - 1));

        glBindTextureUnit(2, brdf_lut);
        engine.default_program->set_int("uBrdfLUT", 2);

        albedo->bind(3);
        engine.default_program->set_int("uAlbedo", 3);

        normal_map->bind(4);
        engine.default_program->set_int("uNormal", 4);

        orm_map->bind(5);
        engine.default_program->set_int("uORM", 5);

        engine.default_circle_program->use();
        irradiance_map.bind(0);
        engine.default_circle_program->set_int("irradianceMap", 0);

        prefilter_map.bind(1);
        engine.default_circle_program->set_int("prefilterMap", 1);
        engine.default_circle_program->set_float("uPrefilterMaxMip", float(prefilter_map.mipLevels() - 1));

        glBindTextureUnit(2, brdf_lut);
        engine.default_circle_program->set_int("uBrdfLUT", 2);

        albedo->bind(3);
        engine.default_circle_program->set_int("uAlbedo", 3);

        normal_map->bind(4);
        engine.default_circle_program->set_int("uNormal", 4);

        orm_map->bind(5);
        engine.default_circle_program->set_int("uORM", 5);
        

        
        
        // voxel_grid->update(&window, &camera);
        // window.draw(voxel_grid, &camera);

        // window.draw(&point_cloud_mesh, &camera);
        
        
        // window.draw(&circle_cloud, &camera);
        window.draw(sphere_mesh, &camera);
        
        
        

        

        ui::begin_frame();
        ui::update_mouse_mode(&window);

        ImGui::Begin("Debug");

        glm::vec3 p = camera.position; // or any vec3

        ImGui::TextColored(ImVec4(1,0.5,0.5,1), "x: %.3f", p.x);
        ImGui::TextColored(ImVec4(0.5,1,0.5,1), "y: %.3f", p.y);
        ImGui::TextColored(ImVec4(0.5,0.5,1,1), "z: %.3f", p.z);
        ImGui::TextColored(ImVec4(0.5,0.5,1,1), "fps: %.3f", fps);

        ImGui::SliderFloat("Relative threshold", &rel_thresh, 0.0f, 10.0f);

        ImGui::End();

        ui::end_frame();
        

        window.swap_buffers();
        engine.poll_events();
    }
    
    ui::shutdown();
}
