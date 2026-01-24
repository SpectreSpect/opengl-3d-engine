#pragma once

#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>

#include "mesh.h"
#include "gridable.h"
#include "voxel_engine/voxel.h"


class VoxelRastorizator {
public:
    Gridable* gridable;

    VoxelRastorizator(Gridable* gridable);
    void rasterize(Mesh* mesh, int vertex_stride = -1);

    static float fmin3(float a, float b, float c);
    static float fmax3(float a, float b, float c);
    static bool plane_box_overlap(const glm::vec3& normal, const glm::vec3& vert, const glm::vec3& maxbox);
    static bool tri_box_overlap(const glm::vec3& boxcenter, const glm::vec3& boxhalf, const glm::vec3& v0,
                                const glm::vec3& v1, const glm::vec3& v2);
    static std::vector<glm::ivec3> rasterize_triangle_to_points(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, float voxel_size);

    // voxel_generator = Voxel* F(glm::ivec3 point)
    template <class F>
    void rasterize_triangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, float voxel_size, F& voxel_generator) {
        std::vector<glm::ivec3> raster = rasterize_triangle_to_points(v0, v1, v2, voxel_size);
        
        std::vector<Voxel> voxels;
        voxels.reserve(raster.size());

        for (auto point : raster) {
            Voxel v = voxel_generator(point);
            voxels.push_back(v);
        }

        gridable->set_voxels(voxels, raster);
    }
};