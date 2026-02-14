#include "point_cloud_frame.h"

PointCloudFrame::PointCloudFrame(const std::filesystem::path& path) {
    load_from_file(path);
}

void PointCloudFrame::load_from_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Failed to open: " + path.string());

    uint32_t count = 0;

    in.read(reinterpret_cast<char*>(&timestamp_ns), sizeof(uint64_t));
    in.read(reinterpret_cast<char*>(&count), sizeof(uint32_t));
    in.read(reinterpret_cast<char*>(&flags), sizeof(uint32_t));
    if (!in) throw std::runtime_error("Bad header in: " + path.string());

    std::vector<PointInstance> local_points;
    local_points.reserve(count);

    const bool has_rgb = (flags & 1u) != 0;
    const bool has_intensity = (flags & 2u) != 0;

    size_t bpp = 3 * sizeof(float);
    if (has_rgb) bpp += 3 * sizeof(uint8_t);
    if (has_intensity) bpp += sizeof(float);

    std::vector<uint8_t> buf(size_t(count) * bpp);
    in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    if (!in) throw std::runtime_error("Unexpected EOF in: " + path.string());

    local_points.resize(count);

    const uint8_t* p = buf.data();
    for (uint32_t i = 0; i < count; ++i) {
        std::memcpy(&local_points[i].pos.x, p, 4); p += 4;
        std::memcpy(&local_points[i].pos.y, p, 4); p += 4;
        std::memcpy(&local_points[i].pos.z, p, 4); p += 4;
        
        float y = local_points[i].pos.y;
        float z = local_points[i].pos.z;

        local_points[i].pos.x = -local_points[i].pos.x;
        local_points[i].pos.y = z;
        local_points[i].pos.z = y;


        //   glm::vec3 pos_0 = glm::vec3(-point_0.x, point_0.z, point_0.y);
        local_points[i].color.r = local_points[i].color.g = local_points[i].color.b = 1.0f;

        if (has_rgb) {
            local_points[i].color.r = *p++;
            local_points[i].color.g = *p++;
            local_points[i].color.b = *p++;
        }
        if (has_intensity) {
            // std::memcpy(&local_points[i].intensity, p, 4); 
            p += 4;
        }
    }

    point_cloud.update_points(std::move(local_points));
}

void PointCloudFrame::draw(RenderState state) {
    state.transform *= get_model_matrix();
    point_cloud.draw(state);
}