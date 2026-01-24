#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../imgui_layer.h"
#include "../voxel_engine/voxel_grid.h"

class TriangleController {
public:
    float range_xz;
    float range_y;

    TriangleController(float range_xz, float range_y);

    bool stick_xz_element(
        const char* id, 
        glm::vec3& p, 
        const ImVec4& col,
        float radius_px, 
        float range_xz
    );

    bool vertex_block_element(
        const char* title, 
        glm::vec3& p, 
        const ImVec4& col,
        float range_xz, 
        float range_y
    );

    template<class F>
    inline void triangle_controller_element( 
        glm::vec3& p0,
        glm::vec3& p1,
        glm::vec3& p2,
        glm::vec3 c0,
        glm::vec3 c1,
        glm::vec3 c2,
        F& on_change
    ) {   
        bool tri_changed = false;
        tri_changed |= vertex_block_element("V0 (red)",   p0, ImVec4(1,0,0,1), range_xz, range_y);
        ImGui::Spacing();
        tri_changed |= vertex_block_element("V1 (green)", p1, ImVec4(0,1,0,1), range_xz, range_y);
        ImGui::Spacing();
        tri_changed |= vertex_block_element("V2 (blue)",  p2, ImVec4(0,0,1,1), range_xz, range_y);

        if (tri_changed)
            on_change(p0, p1, p2);
    };
};