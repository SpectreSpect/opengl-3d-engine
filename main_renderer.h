#pragma once
#include "vulkan/pbr_renderer.h"
#include "skybox/skybox_pass.h"
#include "point_cloud/point_cloud_pass.h"
#include "vulkan_engine.h"


class MainRenderer {
public:
    VulkanEngine* engine = nullptr;
    PBRRenderer pbr_renderer;
    SkyboxPass skybox_pass;
    PointCloudPass point_cloud_pass;

    MainRenderer() = default;
    void create(VulkanEngine& engine);
};