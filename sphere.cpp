#include "sphere.h"


Sphere::Sphere() {
    constexpr float PI = 3.14159265358979323846f;

    float radius = 1.0f;
    int sectors = 64;
    int stacks  = 32;

    struct SphereBuildVertex {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec3 color;
        glm::vec2 uv;

        glm::vec3 tangent_accum   = glm::vec3(0.0f);
        glm::vec3 bitangent_accum = glm::vec3(0.0f);
    };

    std::vector<SphereBuildVertex> temp_vertices;
    std::vector<unsigned int> sphere_indices;

    temp_vertices.reserve((stacks + 1) * (sectors + 1));
    sphere_indices.reserve(stacks * sectors * 6);

    // ------------------------------------------------------------
    // Build sphere vertices
    // ------------------------------------------------------------
    for (int i = 0; i <= stacks; ++i) {
        float stackFrac  = float(i) / float(stacks);
        float stackAngle = PI * 0.5f - stackFrac * PI; // +pi/2 -> -pi/2

        float xy = std::cos(stackAngle);
        float y  = std::sin(stackAngle);

        for (int j = 0; j <= sectors; ++j) {
            float sectorFrac  = float(j) / float(sectors);
            float sectorAngle = sectorFrac * 2.0f * PI;

            float x = xy * std::cos(sectorAngle);
            float z = xy * std::sin(sectorAngle);

            float uvScale = 3.0f;
            float u = sectorFrac * uvScale;
            float v = (1.0f - stackFrac) * uvScale;

            SphereBuildVertex vert;
            vert.pos    = glm::vec3(radius * x, radius * y, radius * z);
            vert.normal = glm::normalize(glm::vec3(x, y, z));
            vert.color  = glm::vec3(1.0f, 1.0f, 1.0f);
            vert.uv     = glm::vec2(u, v);

            temp_vertices.push_back(vert);
        }
    }

    // ------------------------------------------------------------
    // Build sphere indices
    // ------------------------------------------------------------
    for (int i = 0; i < stacks; ++i) {
        int k1 = i * (sectors + 1);
        int k2 = k1 + sectors + 1;

        for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) {
                sphere_indices.push_back(k1);
                sphere_indices.push_back(k2);
                sphere_indices.push_back(k1 + 1);
            }

            if (i != (stacks - 1)) {
                sphere_indices.push_back(k1 + 1);
                sphere_indices.push_back(k2);
                sphere_indices.push_back(k2 + 1);
            }
        }
    }

    // ------------------------------------------------------------
    // Accumulate tangents / bitangents per triangle
    // ------------------------------------------------------------
    for (size_t i = 0; i + 2 < sphere_indices.size(); i += 3) {
        unsigned int i0 = sphere_indices[i + 0];
        unsigned int i1 = sphere_indices[i + 1];
        unsigned int i2 = sphere_indices[i + 2];

        SphereBuildVertex& v0 = temp_vertices[i0];
        SphereBuildVertex& v1 = temp_vertices[i1];
        SphereBuildVertex& v2 = temp_vertices[i2];

        glm::vec3 p0 = v0.pos;
        glm::vec3 p1 = v1.pos;
        glm::vec3 p2 = v2.pos;

        glm::vec2 uv0 = v0.uv;
        glm::vec2 uv1 = v1.uv;
        glm::vec2 uv2 = v2.uv;

        glm::vec3 edge1 = p1 - p0;
        glm::vec3 edge2 = p2 - p0;

        glm::vec2 duv1 = uv1 - uv0;
        glm::vec2 duv2 = uv2 - uv0;

        float denom = duv1.x * duv2.y - duv2.x * duv1.y;
        if (std::abs(denom) < 1e-8f)
            continue;

        float f = 1.0f / denom;

        glm::vec3 tangent =
            f * (duv2.y * edge1 - duv1.y * edge2);

        glm::vec3 bitangent =
            f * (-duv2.x * edge1 + duv1.x * edge2);

        v0.tangent_accum   += tangent;
        v1.tangent_accum   += tangent;
        v2.tangent_accum   += tangent;

        v0.bitangent_accum += bitangent;
        v1.bitangent_accum += bitangent;
        v2.bitangent_accum += bitangent;
    }

    // ------------------------------------------------------------
    // Build final interleaved float array:
    // position(3), normal(3), color(3), uv(2), tangent(4)
    // ------------------------------------------------------------
    std::vector<float> sphere_vertices;
    sphere_vertices.reserve(temp_vertices.size() * 15);

    for (const SphereBuildVertex& v : temp_vertices) {
        glm::vec3 N = glm::normalize(v.normal);

        glm::vec3 T = v.tangent_accum;

        // Fallback if tangent is degenerate
        if (glm::length(T) < 1e-6f) {
            glm::vec3 ref = (std::abs(N.y) < 0.999f)
                ? glm::vec3(0.0f, 1.0f, 0.0f)
                : glm::vec3(1.0f, 0.0f, 0.0f);
            T = glm::normalize(glm::cross(ref, N));
        } else {
            // Gram-Schmidt orthogonalization
            T = glm::normalize(T - N * glm::dot(N, T));
        }

        glm::vec3 B = v.bitangent_accum;
        float handedness = (glm::dot(glm::cross(N, T), B) < 0.0f) ? -1.0f : 1.0f;

        // position
        sphere_vertices.push_back(v.pos.x);
        sphere_vertices.push_back(v.pos.y);
        sphere_vertices.push_back(v.pos.z);

        // normal
        sphere_vertices.push_back(N.x);
        sphere_vertices.push_back(N.y);
        sphere_vertices.push_back(N.z);

        // color
        sphere_vertices.push_back(v.color.x);
        sphere_vertices.push_back(v.color.y);
        sphere_vertices.push_back(v.color.z);

        // uv
        sphere_vertices.push_back(v.uv.x);
        sphere_vertices.push_back(v.uv.y);

        // tangent (xyz + handedness)
        sphere_vertices.push_back(T.x);
        sphere_vertices.push_back(T.y);
        sphere_vertices.push_back(T.z);
        sphere_vertices.push_back(handedness);
    }

    VertexLayout* sphere_vertex_layout = new VertexLayout();
    sphere_vertex_layout->add("position", 0, 3, GL_FLOAT, GL_FALSE, 15 * sizeof(float), 0, 0, {0.0f, 0.0f, 0.0f});
    sphere_vertex_layout->add("normal",   1, 3, GL_FLOAT, GL_FALSE, 15 * sizeof(float), 3 * sizeof(float), 0, {0.0f, 1.0f, 0.0f});
    sphere_vertex_layout->add("color",    2, 3, GL_FLOAT, GL_FALSE, 15 * sizeof(float), 6 * sizeof(float), 0, {1.0f, 1.0f, 1.0f});
    sphere_vertex_layout->add("uv",       3, 2, GL_FLOAT, GL_FALSE, 15 * sizeof(float), 9 * sizeof(float), 0, {0.0f, 0.0f});
    sphere_vertex_layout->add("tangent",  4, 4, GL_FLOAT, GL_FALSE, 15 * sizeof(float), 11 * sizeof(float), 0, {1.0f, 0.0f, 0.0f, 1.0f});

    mesh = new Mesh(sphere_vertices, sphere_indices, sphere_vertex_layout);
}