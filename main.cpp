// main.cpp
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <chrono>
#include <cmath>

#include "engine3d.h"

#include "vao.h"
#include "vbo.h"
#include "ebo.h"
#include "vertex_layout.h"
#include "program.h"
#include "camera.h"
#include "mesh.h"
#include "cube.h"
#include "window.h"
#include "fps_camera_controller.h"
#include "voxel_engine/chunk.h"
#include "voxel_engine/voxel_grid.h"
#include "imgui_layer.h"
#include "voxel_rastorizator.h"


class Grid : public Drawable, public Transformable {
public:
    Cube*** cubes;
    int width;
    int height;
    
    Grid(int width, int height) {
        this->width = width;
        this->height = height;

        cubes = new Cube**[width];

        for (int x = 0; x < width; x++) {
            cubes[x] = new Cube*[height];
            for (int y = 0; y < height; y++) {
                cubes[x][y] = new Cube();
                cubes[x][y]->position.x = x * 2;
                cubes[x][y]->position.z = y * 2;
            }
        }
    }

    void draw(RenderState state) override {
        state.transform *= get_model_matrix();

        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                cubes[x][y]->draw(state);
            }
        }
    }
};

Mesh* create_triangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 color1, glm::vec3 color2, glm::vec3 color3) {
    glm::vec3 e1 = v1 - v0;
    glm::vec3 e2 = v2 - v0;
    glm::vec3 normal  = glm::normalize(glm::cross(e1, e2));

    glm::vec3 points[3] = {v0, v1, v2}; 
    glm::vec3 colors[3] = {color1, color2, color3}; 
    
    std::vector<float> vertices;
    vertices.reserve(3 * 9);

    std::vector<unsigned int> indices = {0, 1, 2};

    for (int i = 0; i < 3; i++) {
        vertices.push_back(points[i].x);
        vertices.push_back(points[i].y);
        vertices.push_back(points[i].z);

        vertices.push_back(normal.x);
        vertices.push_back(normal.y);
        vertices.push_back(normal.z);

        vertices.push_back(colors[i].x);
        vertices.push_back(colors[i].y);
        vertices.push_back(colors[i].z);
    }

    VertexLayout* vertex_layout = new VertexLayout();
    vertex_layout->add({
        0, 3, GL_FLOAT, GL_FALSE,
        9 * sizeof(float),
        0
    });
    vertex_layout->add({
        1, 3, GL_FLOAT, GL_FALSE,
        9 * sizeof(float),
        3 * sizeof(float)
    });
    vertex_layout->add({
        2, 3, GL_FLOAT, GL_FALSE,
        9 * sizeof(float),
        6 * sizeof(float)
    });

    Mesh* triangle = new Mesh(std::move(vertices), std::move(indices), vertex_layout);

    return triangle;
}

template<class F>
void triangle_debug_ui(
    VoxelGrid* voxel_grid, 
    Mesh*& triangle, 
    glm::ivec3 triangle_chunk_pos, 
    float voxel_size,
    glm::vec3& p0,
    glm::vec3& p1,
    glm::vec3& p2,
    glm::vec3 c0,
    glm::vec3 c1,
    glm::vec3 c2,
    F& on_change) {
    auto stick_xz = [&](const char* id, glm::vec3& p, const ImVec4& col,
                        float radius_px, float range_xz) -> bool
    {
        bool changed = false;

        const float knob_r = 6.0f;
        const float r = radius_px;
        const float r_move = (r > knob_r) ? (r - knob_r) : r; // чтобы ручка целиком внутри

        ImVec2 start = ImGui::GetCursorScreenPos();
        ImVec2 size  = ImVec2(r * 2.0f, r * 2.0f);

        ImGui::InvisibleButton(id, size);
        bool active  = ImGui::IsItemActive();
        bool hovered = ImGui::IsItemHovered();

        ImVec2 center = ImVec2(start.x + r, start.y + r);

        // нормализованная позиция из p
        float nx = (range_xz > 0.0f) ? (p.x / range_xz) : 0.0f;
        float nz = (range_xz > 0.0f) ? (p.z / range_xz) : 0.0f;

        // было: clamp -1..1, но nx/nz тут 0..1
        // нужно:
        nx = nx * 2.0f - 1.0f;   // 0..1 -> -1..1
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
            // если хочешь, чтобы вверх по экрану увеличивало Z, то сделай: (-d.y / r_move)

            new_nx = glm::clamp(new_nx, -1.0f, 1.0f);
            new_nz = glm::clamp(new_nz, -1.0f, 1.0f);

            // -1..1 -> 0..range
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
    };



    auto draw_vertex_block = [&](const char* title, glm::vec3& p, const ImVec4& col,
                                    float range_xz, float range_y) -> bool
    {
        bool changed = false;

        // Чтобы “стартовые значения” не выглядели странно, когда p вне диапазона:
        // (если не хочешь клампить — закомментируй)
        p.x = glm::clamp(p.x, 0.0f, range_xz);
        p.z = glm::clamp(p.z, 0.0f, range_xz);
        p.y = glm::clamp(p.y, 0.0f, range_y);

        const float r = 55.0f;
        const float h =
            ImGui::GetTextLineHeightWithSpacing() +          // заголовок
            (r * 2.0f) +                                     // стик
            ImGui::GetTextLineHeightWithSpacing() +          // строка X/Z
            ImGui::GetFrameHeightWithSpacing() +             // слайдер Y
            ImGui::GetStyle().WindowPadding.y * 2.0f +
            ImGui::GetStyle().ItemSpacing.y * 3.0f;

        ImGui::PushStyleColor(ImGuiCol_Border, col);
        ImGui::BeginChild(title, ImVec2(0, h), true);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();

        ImGui::PushID(title);

        // Стик XZ
        changed |= stick_xz("stick", p, col, r, range_xz);

        ImGui::Text("X: %.2f   Z: %.2f", p.x, p.z);

        // Y слайдер (красим “граб”)
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, col);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, col);
        changed |= ImGui::SliderFloat("Y", &p.y, 0.0f, range_y, "%.3f");
        ImGui::PopStyleColor(2);

        ImGui::PopID();

        ImGui::EndChild();
        return changed;
    };

    float range_xz = glm::max(voxel_grid->chunk_size.x, voxel_grid->chunk_size.z) * voxel_size;
    float range_y  = voxel_grid->chunk_size.y * voxel_size;
    
    bool tri_changed = false;
    tri_changed |= draw_vertex_block("V0 (red)",   p0, ImVec4(1,0,0,1), range_xz, range_y);
    ImGui::Spacing();
    tri_changed |= draw_vertex_block("V1 (green)", p1, ImVec4(0,1,0,1), range_xz, range_y);
    ImGui::Spacing();
    tri_changed |= draw_vertex_block("V2 (blue)",  p2, ImVec4(0,0,1,1), range_xz, range_y);

    glm::vec3 chunk_origin = glm::vec3(triangle_chunk_pos) * glm::vec3(voxel_grid->chunk_size) * voxel_size;

    glm::vec3 out_p0 = chunk_origin + p0;
    glm::vec3 out_p1 = chunk_origin + p1;
    glm::vec3 out_p2 = chunk_origin + p2;

    if (tri_changed) {
        on_change();
        delete triangle; 
        triangle = create_triangle(out_p0, out_p1, out_p2, c0, c1, c2);
    }
}

float clear_col[4] = {0.776470588f, 0.988235294f, 1.0f, 1.0f};

int main() {
    Engine3D* engine = new Engine3D();
    Window* window = new Window(engine, 1280, 720, "3D visualization");
    engine->set_window(window);
    ui::init(window->window);
    
    window->disable_cursor();

    Camera* camera = new Camera();
    window->set_camera(camera);

    FPSCameraController* camera_controller = new FPSCameraController(camera);
    camera_controller->speed = 20;

    float timer = 0;
    float lastFrame = 0;

    VoxelGrid* voxel_grid = new VoxelGrid({16, 16, 16}, {24, 6, 24});
    // VoxelGrid* voxel_grid = new VoxelGrid({16, 16, 16}, {12, 12, 12});

    float voxel_size = 1.0f;
    glm::vec3 left_top_front = glm::vec3(
        0,
        voxel_grid->chunk_size.y * voxel_size,
        voxel_grid->chunk_size.z * voxel_size
    );

    glm::vec3 right_top_front = glm::vec3(
        voxel_grid->chunk_size.x * voxel_size,
        voxel_grid->chunk_size.y * voxel_size,
        voxel_grid->chunk_size.z * voxel_size
    );

    glm::vec3 bottom_center = glm::vec3(
        voxel_grid->chunk_size.x * voxel_size / 2.0f,
        0.0f,
        0.0f
    );

    glm::vec3 p0 = left_top_front;
    glm::vec3 p1 = right_top_front;
    glm::vec3 p2 = bottom_center;

    glm::vec3 c0(1.0f, 0.0f, 0.0f);
    glm::vec3 c1(0.0f, 1.0f, 0.0f);
    glm::vec3 c2(0.0f, 0.0f, 1.0f);

    glm::ivec3 triangle_chunk_pos = glm::ivec3(0, 2, 0);

    glm::vec3 chunk_origin = glm::vec3(triangle_chunk_pos) * glm::vec3(voxel_grid->chunk_size) * voxel_size;

    glm::vec3 out_p0 = chunk_origin + p0;
    glm::vec3 out_p1 = chunk_origin + p1;
    glm::vec3 out_p2 = chunk_origin + p2;

    Mesh* triangle = create_triangle(out_p0, out_p1, out_p2, c0, c1, c2);

    VoxelRastorizator* voxel_rastorizator = new VoxelRastorizator(nullptr);

    glm::vec3 prev_cam_pos = camera_controller->camera->position;
    while(window->is_open()) {
        float currentFrame = (float)glfwGetTime();
        float delta_time = currentFrame - lastFrame;
        timer += delta_time;
        lastFrame = currentFrame;   

        ui::begin_frame();
        ui::update_mouse_mode(window);

        camera_controller->update(window, delta_time);

        window->clear_color({clear_col[0], clear_col[1], clear_col[2], clear_col[3]});

        voxel_grid->update(window, camera);
        window->draw(voxel_grid, camera);
        window->draw(triangle, camera);

        glm::vec3 velocity = (camera_controller->camera->position - prev_cam_pos) / delta_time;

        ImGui::Begin("Debug");

        auto on_change_triangle = [&]() {
            auto key = voxel_grid->pack_key(triangle_chunk_pos.x, triangle_chunk_pos.y, triangle_chunk_pos.z);

            auto it = voxel_grid->chunks.find(key);
            if (it == voxel_grid->chunks.end()) return 1;

            Chunk* chunk = it->second;
            voxel_rastorizator->gridable = chunk;

            const glm::ivec3 cs = voxel_grid->chunk_size;
            const int chunk_volume = cs.x * cs.y * cs.z;

            std::vector<glm::ivec3> voxel_positions;
            voxel_positions.reserve(chunk_volume);

            std::vector<Voxel> voxels;
            voxels.reserve(chunk_volume);

            for (int x = 0; x < cs.x; x++)
            for (int y = 0; y < cs.y; y++)
            for (int z = 0; z < cs.z; z++) {
                voxel_positions.emplace_back(x, y, z);
                Voxel v{};
                v.visible = false;
                voxels.push_back(v);
            }

            auto raster = voxel_rastorizator->rasterize_triangle_to_points(
                chunk_origin + p0,
                chunk_origin + p1,
                chunk_origin + p2,
                voxel_size
            );

            glm::ivec3 chunk_origin_vox = triangle_chunk_pos * cs;

            auto bary = [](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& p) {
                glm::vec3 v0 = b - a;
                glm::vec3 v1 = c - a;
                glm::vec3 v2 = p - a;

                float d00 = glm::dot(v0, v0);
                float d01 = glm::dot(v0, v1);
                float d11 = glm::dot(v1, v1);
                float d20 = glm::dot(v2, v0);
                float d21 = glm::dot(v2, v1);

                float denom = d00 * d11 - d01 * d01;
                if (std::abs(denom) < 1e-8f) return glm::vec3(-1.0f);

                float v = (d11 * d20 - d01 * d21) / denom;
                float w = (d00 * d21 - d01 * d20) / denom;
                float u = 1.0f - v - w;
                return glm::vec3(u, v, w);
            };

            for (auto gp : raster) {
                glm::ivec3 lp = gp - chunk_origin_vox;
                if (lp.x < 0 || lp.y < 0 || lp.z < 0 || lp.x >= cs.x || lp.y >= cs.y || lp.z >= cs.z)
                    continue;

                voxel_positions.push_back(lp);

                glm::vec3 P = (glm::vec3(lp) + 0.5f) * voxel_size;

                glm::vec3 w = bary(p0, p1, p2, P);
                if (w.x < 0.0f) continue;

                w = glm::clamp(w, glm::vec3(0.0f), glm::vec3(1.0f));
                float s = w.x + w.y + w.z;
                if (s > 0.0f) w /= s;

                Voxel v{};
                v.visible = true;
                v.color = c0 * w.x + c1 * w.y + c2 * w.z;
                voxels.push_back(v);
            }

            chunk->set_voxels(voxels, voxel_positions);
            voxel_grid->chunks_to_update.insert(key);
        };

        triangle_debug_ui(voxel_grid, triangle, triangle_chunk_pos, voxel_size, p0, p1, p2, c0, c1, c2, on_change_triangle);

        if (ImGui::Button("Rasterize the triangle")) {

        }


        ImGui::End();

        ui::end_frame();

        window->swap_buffers();
        engine->poll_events();

        prev_cam_pos = camera_controller->camera->position;
    }
    
    ui::shutdown();
}
