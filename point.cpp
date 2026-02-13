#include "point.h"
#include "engine3d.h"
#include <filesystem>

Point::Point() {
    // Quad corners in "local sprite space"
    // corner.x, corner.y in [-1..+1]
    const float quadCorners[] = {
        -1.0f, -1.0f,  // v0
        -1.0f, +1.0f,  // v1
        +1.0f, -1.0f,  // v2
        +1.0f, +1.0f   // v3
    };

    const unsigned int quadIndices[] = {
        0, 1, 2,
        2, 1, 3
    };

    PointInstance dummy{};
    dummy.pos = {0,0,0};

    vao = new VAO();
    quad_vbo = new VBO(quadCorners, sizeof(quadCorners));
    quad_ebo = new EBO(quadIndices, sizeof(quadIndices));
    instance_vbo = new VBO(&dummy, sizeof(PointInstance));

    vao->bind();
    quad_ebo->bind();

    // Per-vertex corner attribute (location = 1)
    quad_vbo->bind();
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE,
        2 * sizeof(float),
        (void*)0
    );
    glVertexAttribDivisor(1, 0);

    // Per-instance position attribute (location = 0)
    instance_vbo->bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE,
        sizeof(PointInstance),
        (void*)offsetof(PointInstance, pos)
    );
    glVertexAttribDivisor(0, 1);

    vao->unbind();

    instance_count = 0;
}

Point::~Point() {
    delete vao;
    delete quad_vbo;
    delete quad_ebo;
    delete instance_vbo;
}

void Point::set_points(const std::vector<PointInstance>& points) {
    instance_count = (int)points.size();
    if (instance_count == 0) return;

    instance_vbo->update_mapped(points.data(),
                               points.size() * sizeof(PointInstance),
                               GL_STREAM_DRAW);
}

void Point::draw(RenderState state) {
    if (instance_count <= 0) return;

    // You can reuse your line program if you want,
    // but typically you'd have a separate point shader program:
    Program* program = state.engine->default_point_program;
    program->use();

    program->set_mat4("uProj", state.proj);
    if (state.camera) {
        program->set_mat4("uView", state.camera->get_view_matrix());
    }

    // TODO: replace with your real window size (grab from engine if you have it)
    float window_width = 1280;
    float window_height = 720;

    program->set_vec2("uViewport", glm::vec2(window_width, window_height));
    program->set_float("uPointSizePx", size);
    program->set_vec4("uColor", glm::vec4(color, 1.0f));
    program->set_int("uRound", round ? 1 : 0);

    vao->bind();
    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, instance_count);
    vao->unbind();
}
