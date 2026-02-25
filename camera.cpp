# include "camera.h"


Camera::Camera(glm::vec3 pos, glm::vec3 up_vec, float fov_deg)
{
    this->position = pos;
    this->up = up_vec;
    this->fov = fov_deg;
    // front = {0.0f, 0.0f, 1.0f};
    front = {0.0f, 0.0f, -1.0f};

    create_clusters(clusters, num_clusters, glm::radians(this->fov), 1280.0f / 720.0f, near, far);
}

glm::mat4 Camera::get_view_matrix() const {
    return glm::lookAt(position, position + front, up);
}

glm::mat4 Camera::get_projection_matrix(float aspect_ratio) const {
    return glm::perspective(glm::radians(fov), aspect_ratio, near, far);
}

void Camera::computeSliceDistancesLinear(float nearPlane, float farPlane, unsigned zSlices, std::vector<float>& out) {
    out.resize(zSlices + 1);
    for (unsigned i = 0; i <= zSlices; ++i) {
        float t = float(i) / float(zSlices);
        out[i] = glm::mix(nearPlane, farPlane, t);
    }
}

// Helper: compute logarithmic slice distances (good for large depth ranges)
void Camera::computeSliceDistancesLog(float nearPlane, float farPlane, unsigned zSlices, std::vector<float>& out) {
    out.resize(zSlices + 1);
    float lnN = std::log(nearPlane);
    float lnF = std::log(farPlane);
    for (unsigned i = 0; i <= zSlices; ++i) {
        float t = float(i) / float(zSlices);
        out[i] = std::exp(glm::mix(lnN, lnF, t));
    }
}


void Camera::create_clusters_full(
    std::vector<AABB>& out_cluster_cells,
    glm::uvec3 numClusters,        // xTiles, yTiles, zSlices (use unsigned ints)
    float fovY_radians,
    float aspect,
    const std::vector<float>& sliceDistances) 
{
    unsigned xTiles = numClusters.x;
    unsigned yTiles = numClusters.y;
    unsigned zSlices = numClusters.z;
    out_cluster_cells.clear();
    out_cluster_cells.resize((size_t)xTiles * yTiles * zSlices);

    const float tanHalfFovY = tanf(fovY_radians * 0.5f);

    for (unsigned z = 0; z < zSlices; ++z) {
        float d0 = sliceDistances[z];     // near bound for slice (positive)
        float d1 = sliceDistances[z+1];   // far  bound
        // half heights/widths at both depths
        float hh0 = d0 * tanHalfFovY;
        float hh1 = d1 * tanHalfFovY;
        float hw0 = hh0 * aspect;
        float hw1 = hh1 * aspect;

        for (unsigned y = 0; y < yTiles; ++y) {
            // normalized v coordinates (0..1). v=0 bottom, v=1 top
            float v0 = float(y) / float(yTiles);
            float v1 = float(y + 1) / float(yTiles);

            for (unsigned x = 0; x < xTiles; ++x) {
                float u0 = float(x) / float(xTiles);
                float u1 = float(x + 1) / float(xTiles);

                // compute 8 corners of frustum cell in view space (z = -d)
                glm::vec3 corners[8];

                // near plane corners (d0)
                float xs0 = (u0 * 2.0f - 1.0f) * hw0;
                float xs1 = (u1 * 2.0f - 1.0f) * hw0;
                float ys0 = (v0 * 2.0f - 1.0f) * hh0;
                float ys1 = (v1 * 2.0f - 1.0f) * hh0;
                corners[0] = glm::vec3(xs0, ys0, -d0);
                corners[1] = glm::vec3(xs1, ys0, -d0);
                corners[2] = glm::vec3(xs1, ys1, -d0);
                corners[3] = glm::vec3(xs0, ys1, -d0);

                // far plane corners (d1)
                xs0 = (u0 * 2.0f - 1.0f) * hw1;
                xs1 = (u1 * 2.0f - 1.0f) * hw1;
                ys0 = (v0 * 2.0f - 1.0f) * hh1;
                ys1 = (v1 * 2.0f - 1.0f) * hh1;
                corners[4] = glm::vec3(xs0, ys0, -d1);
                corners[5] = glm::vec3(xs1, ys0, -d1);
                corners[6] = glm::vec3(xs1, ys1, -d1);
                corners[7] = glm::vec3(xs0, ys1, -d1);

                // create AABB from these 8 points
                glm::vec3 bmin( std::numeric_limits<float>::infinity() );
                glm::vec3 bmax( -std::numeric_limits<float>::infinity() );
                for (int i = 0; i < 8; ++i) {
                    bmin = glm::min(bmin, corners[i]);
                    bmax = glm::max(bmax, corners[i]);
                }

                // small epsilon padding (optional) to avoid numerical misses:
                const float eps = 1e-4f * (d0 + d1) * 0.5f; // scale with depth a bit
                bmin -= glm::vec3(eps);
                bmax += glm::vec3(eps);

                size_t idx = size_t(x) + size_t(y) * xTiles + size_t(z) * xTiles * yTiles;
                out_cluster_cells[idx].min = glm::vec4(bmin.x, bmin.y, bmin.z, 1.0f);
                out_cluster_cells[idx].max = glm::vec4(bmax.x, bmax.y, bmax.z, 1.0f);
            }
        }
    }
}

void Camera::create_clusters(std::vector<AABB>& out_cluster_cells, glm::vec3 num_clusters,
                               float fovY_radians, float aspect, float nearPlane, float farPlane,
                               bool useLogSlices)
{
    useLogSlices = true;
    // convert glm::vec3 -> glm::uvec3
    glm::uvec3 tiles = glm::uvec3((unsigned)num_clusters.x, (unsigned)num_clusters.y, (unsigned)num_clusters.z);

    std::vector<float> sliceDistances;
    if (useLogSlices) computeSliceDistancesLog(nearPlane, farPlane, tiles.z, sliceDistances);
    else              computeSliceDistancesLinear(nearPlane, farPlane, tiles.z, sliceDistances);

    create_clusters_full(out_cluster_cells, tiles, fovY_radians, aspect, sliceDistances);
}

// void Camera::processMouseMovement(float xoffset, float yoffset) {
//     float sensitivity = 0.15f;
//     xoffset *= sensitivity;
//     yoffset *= sensitivity;

//     yaw += xoffset;
//     pitch += yoffset;

//     if (pitch > 89.0f) pitch = 89.0f;
//     if (pitch < -89.0f) pitch = -89.0f;

//     glm::vec3 front;
//     front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
//     front.y = sin(glm::radians(pitch));
//     front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
//     this->front = glm::normalize(front);
// }
