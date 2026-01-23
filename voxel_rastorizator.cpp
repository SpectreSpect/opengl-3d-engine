#include "voxel_rastorizator.h"


VoxelRastorizator::VoxelRastorizator(Gridable* gridable) {
    this->gridable = gridable;
}

void VoxelRastorizator::rasterize(Mesh* mesh, int vertex_stride) {

}

std::vector<glm::ivec3> VoxelRastorizator::move_discretely_along_line(glm::vec3 start, glm::vec3 end, float voxel_size) {
    glm::ivec3 voxel_pos = glm::ivec3(floor(start.x / voxel_size), floor(start.y / voxel_size), floor(start.z / voxel_size));
    glm::vec3 edge1 = end - start;
    glm::ivec3 count_voxels = glm::ivec3(ceil(edge1.x / voxel_size), ceil(edge1.y / voxel_size), ceil(edge1.z / voxel_size));
    glm::vec3 step = glm::vec3(edge1.x / count_voxels.x, edge1.y / count_voxels.y, edge1.z / count_voxels.z);

    std::vector<glm::ivec3> visited_voxels;
    visited_voxels.reserve(count_voxels.x * count_voxels.y * count_voxels.z);

    visited_voxels.push_back(voxel_pos);
    for (int i = 0; i < count_voxels.x; i++) {
        for (int j = 0; j < count_voxels.y; j++) {
            for (int k = 0; k < count_voxels.z; k++) {
                voxel_pos += step;
                visited_voxels.push_back(voxel_pos);
            }
        }
    }

    return visited_voxels;
}

std::vector<glm::ivec3> VoxelRastorizator::rasterize_triangle_to_points(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, float voxel_size) {
    std::vector<glm::ivec3> v0_to_v1_points = VoxelRastorizator::move_discretely_along_line(v0, v1, voxel_size);
    std::vector<glm::ivec3> v2_to_v1_points = VoxelRastorizator::move_discretely_along_line(v2, v1, voxel_size);

    std::vector<glm::ivec3> dense_points_line = v0_to_v1_points.size() > v2_to_v1_points.size() ? v0_to_v1_points : v2_to_v1_points;
    std::vector<glm::ivec3> not_dense_points_line = v0_to_v1_points.size() > v2_to_v1_points.size() ? v2_to_v1_points : v0_to_v1_points;

    std::vector<glm::ivec3> visited_points;
    visited_points.reserve(dense_points_line.size() + not_dense_points_line.size());

    int extra_count = dense_points_line.size() / not_dense_points_line.size();
    int remainder = dense_points_line.size() % not_dense_points_line.size();
    for (size_t not_dense_idx = 0; not_dense_idx < not_dense_points_line.size(); not_dense_idx++) {
        for (size_t i = 0; i < extra_count; i++) {
            size_t dense_idx = not_dense_idx * extra_count + i;
            auto& line = move_discretely_along_line(not_dense_points_line[not_dense_idx], dense_points_line[dense_idx], voxel_size);
            visited_points.insert(visited_points.end(), line.begin(), line.end());
        }
    }

    if (remainder > 0) {
        size_t start_dense_idx = not_dense_points_line.size() * extra_count;
        for (size_t dense_idx = start_dense_idx; dense_idx < dense_points_line.size(); dense_idx++) {
            auto& line = move_discretely_along_line(not_dense_points_line.back(), dense_points_line[dense_idx], voxel_size);
            visited_points.insert(visited_points.end(), line.begin(), line.end());
        }
    }

    return visited_points;
}