#include "point_cloud_video.h"

void PointCloudVideo::load_from_file(const std::filesystem::path& csv_path) {
    std::ifstream in(csv_path);
    if (!in) throw std::runtime_error("Failed to open: " + csv_path.string());

    std::vector<IndexEntry> entries;
    std::filesystem::path video_dir_path = csv_path.parent_path();
    std::string line;

    while (std::getline(in, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string a, b, c, d;

        if (!std::getline(ss, a, ',')) continue;
        if (!std::getline(ss, b, ',')) continue;
        if (!std::getline(ss, c, ',')) continue;
        if (!std::getline(ss, d, ',')) continue;

        auto is_uint = [](const std::string& s) {
            if (s.empty()) return false;
            for (char ch : s) if (ch < '0' || ch > '9') return false;
            return true;
        };
        if (!is_uint(a) || !is_uint(b) || !is_uint(d)) continue;

        IndexEntry e;
        e.frame_id = std::stoull(a);
        e.timestamp_ns = std::stoull(b);
        e.filename = c;
        e.point_count = static_cast<uint32_t>(std::stoul(d));

        entries.push_back(std::move(e));
    }

    std::sort(entries.begin(), entries.end(),
              [](const IndexEntry& x, const IndexEntry& y) { return x.timestamp_ns < y.timestamp_ns; });
    
    frames.clear();
    frames.reserve(entries.size());

    for (const auto& e : entries) {
        frames.emplace_back();
        frames.back().load_from_file(video_dir_path / e.filename);
    }
}

size_t PointCloudVideo::get_frame_id(float time, size_t search_start_id) {
    int frame_id = 0;
    for (int i = search_start_id; i < frames.size(); i++) {
        double time_seconds = (double)frames[i].timestamp_ns / 1000000000.0f;

        if (time_seconds > timer)
            return frame_id;
        else
            frame_id = i;
    }

    return frame_id;
}

void PointCloudVideo::move(float time) {
    timer = time;
    current_frame = get_frame_id(timer);
}

void PointCloudVideo::pause() {
    paused = true;
}

void PointCloudVideo::resume() {
    paused = false;
}

void PointCloudVideo::update(float delta_time) {
    if (paused) return;

    if (looped && current_frame == frames.size() - 1) {
        current_frame = 0;
        timer = 0.0f;
    }

    timer += delta_time;
    current_frame = get_frame_id(timer, current_frame);
}

void PointCloudVideo::draw(RenderState state) {
    state.transform *= get_model_matrix();

    if (current_frame > frames.size())
        throw std::out_of_range("PointCloudVideo::draw");
    
    frames[current_frame].draw(state);
}