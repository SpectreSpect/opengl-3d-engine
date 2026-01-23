#pragma once

#include <iostream>
#include <algorithm>

#include "mesh.h"
#include "gridable.h"
#include "voxel_engine/voxel.h"

class VoxelRastorizator {
public:
    Gridable* gridable;

    VoxelRastorizator(Gridable* gridable);
    void rasterize(Mesh* mesh, int vertex_stride = -1);
    static std::vector<glm::ivec3> move_discretely_along_line(glm::vec3 start, glm::vec3 end, float voxel_size);
    static std::vector<glm::ivec3> rasterize_triangle_to_points(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, float voxel_size);

    // voxel_generator = Voxel* F(glm::ivec3 point)
    template <class F>
    void rasterize_triangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, float voxel_size, F& voxel_generator) {
        std::vector<glm::ivec3> points = VoxelRastorizator::rasterize_triangle_to_points(v0, v1, v2, voxel_size);
        points.erase(std::unique(points.begin(), points.end()), points.end());

        for (auto& point : points) {
            Voxel* voxel = voxel_generator(point);
            gridable->set_voxel(voxel);
        }
    }
};