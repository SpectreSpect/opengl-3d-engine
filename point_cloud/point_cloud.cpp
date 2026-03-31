#include "point_cloud.h"

void PointCloud::create(VulkanEngine& engine) {
    this->engine = &engine;

    instance_buffer = VideoBuffer(engine, sizeof(PointInstance));
}

void PointCloud::set_points(const std::vector<PointInstance>& point_instances) {
    num_instances = (uint32_t)point_instances.size();
    if (num_instances == 0) return;

    instance_buffer.update_data(point_instances.data(), point_instances.size() * sizeof(PointInstance));
}