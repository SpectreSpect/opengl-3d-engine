#include "triangle_controller.h"

TriangleController::TriangleController(float range_xz, float range_y) {
    this->range_xz = range_xz;
    this->range_y = range_y;
}

bool TriangleController::stick_xz_element(
    const char* id, 
    glm::vec3& p, 
    const ImVec4& col,
    float radius_px, 
    float range_xz
) {
    bool changed = false;

    const float knob_r = 6.0f;
    const float r = radius_px;
    const float r_move = (r > knob_r) ? (r - knob_r) : r;

    ImVec2 start = ImGui::GetCursorScreenPos();
    ImVec2 size  = ImVec2(r * 2.0f, r * 2.0f);

    ImGui::InvisibleButton(id, size);
    bool active  = ImGui::IsItemActive();
    bool hovered = ImGui::IsItemHovered();

    ImVec2 center = ImVec2(start.x + r, start.y + r);

    float nx = (range_xz > 0.0f) ? (p.x / range_xz) : 0.0f;
    float nz = (range_xz > 0.0f) ? (p.z / range_xz) : 0.0f;

    nx = nx * 2.0f - 1.0f;
    nz = nz * 2.0f - 1.0f;
    nx = glm::clamp(nx, -1.0f, 1.0f);
    nz = glm::clamp(nz, -1.0f, 1.0f);

    ImVec2 knob = ImVec2(center.x + nx * r_move, center.y + nz * r_move);

    if (active) {
        ImVec2 m = ImGui::GetIO().MousePos;
        ImVec2 d = ImVec2(m.x - center.x, m.y - center.y);

        float len = std::sqrt(d.x*d.x + d.y*d.y);
        if (len > r_move && len > 0.0f) {
            d.x = d.x / len * r_move;
            d.y = d.y / len * r_move;
        }

        float new_nx = (r_move > 0.0f) ? (d.x / r_move) : 0.0f;
        float new_nz = (r_move > 0.0f) ? (d.y / r_move) : 0.0f;

        new_nx = glm::clamp(new_nx, -1.0f, 1.0f);
        new_nz = glm::clamp(new_nz, -1.0f, 1.0f);

        float new_x = (new_nx + 1.0f) * 0.5f * range_xz;
        float new_z = (new_nz + 1.0f) * 0.5f * range_xz;


        if (new_x != p.x || new_z != p.z) {
            p.x = new_x;
            p.z = new_z;
            changed = true;
        }

        knob = ImVec2(center.x + d.x, center.y + d.y);
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 col_border = ImGui::GetColorU32(ImVec4(col.x, col.y, col.z, 0.9f));
    ImU32 col_bg     = ImGui::GetColorU32(ImVec4(0, 0, 0, hovered ? 0.25f : 0.15f));
    ImU32 col_knob   = ImGui::GetColorU32(ImVec4(col.x, col.y, col.z, active ? 1.0f : 0.85f));

    dl->AddRectFilled(start, ImVec2(start.x + size.x, start.y + size.y), col_bg, 10.0f);
    dl->AddCircle(center, r, col_border, 0, 2.0f);
    dl->AddLine(ImVec2(center.x - r, center.y), ImVec2(center.x + r, center.y), col_border, 1.0f);
    dl->AddLine(ImVec2(center.x, center.y - r), ImVec2(center.x, center.y + r), col_border, 1.0f);

    dl->AddCircleFilled(knob, knob_r, col_knob);

    return changed;
}

bool TriangleController::vertex_block_element(
    const char* title, 
    glm::vec3& p, 
    const ImVec4& col,
    float range_xz, 
    float range_y
) {
    bool changed = false;

    p.x = glm::clamp(p.x, 0.0f, range_xz);
    p.z = glm::clamp(p.z, 0.0f, range_xz);
    p.y = glm::clamp(p.y, 0.0f, range_y);

    const float r = 55.0f;
    const float h =
        ImGui::GetTextLineHeightWithSpacing() +
        (r * 2.0f) +
        ImGui::GetTextLineHeightWithSpacing() +
        ImGui::GetFrameHeightWithSpacing() +
        ImGui::GetStyle().WindowPadding.y * 2.0f +
        ImGui::GetStyle().ItemSpacing.y * 3.0f;

    ImGui::PushStyleColor(ImGuiCol_Border, col);
    ImGui::BeginChild(title, ImVec2(0, h), true);
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();

    ImGui::PushID(title);

    changed |= stick_xz_element("stick", p, col, r, range_xz);

    ImGui::Text("X: %.2f   Z: %.2f", p.x, p.z);

    ImGui::PushStyleColor(ImGuiCol_SliderGrab, col);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, col);
    changed |= ImGui::SliderFloat("Y", &p.y, 0.0f, range_y, "%.3f");
    ImGui::PopStyleColor(2);

    ImGui::PopID();

    ImGui::EndChild();
    return changed;
}