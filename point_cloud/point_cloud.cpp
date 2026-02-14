#include "point_cloud.h"


void PointCloud::update_points(std::vector<PointInstance> points) {
    this->points = std::move(points);
    needs_gpu_sync = true;
}

void PointCloud::set_point(unsigned int id, PointInstance value) {
    if (id >= points.size()) {
        std::out_of_range("PointCloud::set_point");
        return;
    }
    
    points[id] = value;
    needs_gpu_sync = true;
}

PointInstance PointCloud::get_point(unsigned int id) {
    if (id >= points.size())
        std::out_of_range("PointCloud::set_point");

    return points[id];
}

void PointCloud::sync_gpu() {
    if(!needs_gpu_sync) return;
    point_renderer.set_points(points);
    needs_gpu_sync = false;
}

size_t PointCloud::size() {
    return points.size();
}

void PointCloud::draw(RenderState state) {
    state.transform *= get_model_matrix();
    sync_gpu();
    point_renderer.draw(state);
}

