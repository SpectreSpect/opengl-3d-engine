#pragma once
#include "vao.h"
#include "vbo.h"
#include "ebo.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstddef> // offsetof
#include "drawable.h"

struct PointInstance {
    glm::vec3 pos;
    glm::vec3 color;  // per-point color (RGB 0..1)
};

class Point : public Drawable {
public:
    Point();
    ~Point();

    float size_px = 6.0f;

    // world-space (your scene units, e.g. meters)
    float size_world = 0.05f;

    // true = constant pixel size, false = world-space size
    bool constant_screen_size = true;

    bool round = true;

    void set_points(const std::vector<PointInstance>& points);
    void draw(RenderState state) override;

private:
    VAO* vao = nullptr;
    VBO* quad_vbo = nullptr;       // static: 4 corners
    EBO* quad_ebo = nullptr;       // static: 6 indices
    VBO* instance_vbo = nullptr;   // dynamic: pos per point
    int instance_count = 0;
};
