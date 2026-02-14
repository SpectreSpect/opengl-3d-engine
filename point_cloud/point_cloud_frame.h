#pragma once 
#include "point_cloud.h"
#include <cstring>

class PointCloudFrame : public Drawable, public Transformable {
public:
    uint64_t timestamp_ns = 0;
    uint32_t flags = 0;        // 1=rgb, 2=intensity
    PointCloud point_cloud;

    PointCloudFrame() = default;
    PointCloudFrame(const std::filesystem::path& path);
    void load_from_file(const std::filesystem::path& path);

    void draw(RenderState state) override;
};