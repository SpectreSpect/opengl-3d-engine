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
};

class Point : public Drawable {
public:
    Point();
    ~Point();

    glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
    float size = 6.0f;        // diameter in pixels
    bool round = true;        // circle vs square

    void set_points(const std::vector<PointInstance>& points);
    void draw(RenderState state) override;

private:
    VAO* vao = nullptr;
    VBO* quad_vbo = nullptr;       // static: 4 corners
    EBO* quad_ebo = nullptr;       // static: 6 indices
    VBO* instance_vbo = nullptr;   // dynamic: pos per point
    int instance_count = 0;
};
