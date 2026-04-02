#include "point_cloud.h"

void PointCloud::create(VulkanEngine& engine) {
    this->engine = &engine;
    instance_buffer = VideoBuffer(engine, sizeof(PointInstance));
    external_instance_buffer = nullptr;
}

void PointCloud::set_points(const std::vector<PointInstance>& point_instances) {
    external_instance_buffer = nullptr;

    num_instances = static_cast<uint32_t>(point_instances.size());
    if (num_instances == 0) return;

    instance_buffer.update_data(
        point_instances.data(),
        point_instances.size() * sizeof(PointInstance)
    );
}

void PointCloud::set_points(VideoBuffer& point_instance_buffer, uint32_t num_instances) {
    this->num_instances = num_instances;
    external_instance_buffer = &point_instance_buffer;
}