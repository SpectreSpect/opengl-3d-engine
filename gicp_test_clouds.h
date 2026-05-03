#pragma once
#include "point_cloud/point_cloud_video.h"


class GICPTestClouds {
public:
    GICPTestClouds() = default;
    void create_roads(VulkanEngine* engine);
    void generate_road(VulkanEngine* engine, PointCloudFrame* frame, glm::vec4 color);
    
    void add_point(PointCloudFrame* frame, const glm::vec4& pos, const glm::vec4& color, const glm::vec4& normal);

    PointCloudFrame target_frame;
    PointCloudFrame source_frame;
};