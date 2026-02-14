#pragma once
#include "point_cloud_frame.h"
#include <algorithm>

struct IndexEntry {
    uint64_t frame_id = 0;
    uint64_t timestamp_ns = 0;
    std::filesystem::path filename;
    uint32_t point_count = 0;
};

class PointCloudVideo : public Drawable, public Transformable {
public:
    std::vector<PointCloudFrame> frames;
    size_t current_frame = 0;
    size_t last_frame_id = 0;
    float timer = 0.0f;
    bool paused = false;
    bool looped = true;

    PointCloudVideo() = default;
    void load_from_file(const std::filesystem::path& csv_path);
    size_t get_frame_id(float time, size_t search_start_id = 0);
    void move(float time = 0.0f);
    void pause();
    void resume();
    void update(float delta_time);
    virtual void draw(RenderState state) override;
};