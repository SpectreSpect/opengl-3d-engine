#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <vector>
#include <fstream>
#include "../transformable.h"
#include "../drawable.h"
#include "../point.h"

class PointCloud : public Drawable, public Transformable{
public:
    PointCloud() = default;
    void update_points(std::vector<PointInstance> points);
    void set_point(unsigned int id, PointInstance value);
    PointInstance get_point(unsigned int id);
    size_t size();
    void draw(RenderState state) override;

private:
    bool needs_gpu_sync = true;
    std::vector<PointInstance> points;
    Point point_renderer;

    void sync_gpu();
};