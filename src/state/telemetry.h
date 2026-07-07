#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace App {

struct SubLayerNode {
    std::string name;
    std::string path;
    std::vector<SubLayerNode> children;
    bool enumerated = false;
};

struct Status {
    std::string current_ifs_path;
    std::string playing_animation;
    std::string last_error;
    std::string active_label;

    struct AfpLabel {
        std::string name;
        int frame = 0;
    };
    std::vector<AfpLabel> labels;

    struct McChild {
        std::string name;
        float x = 0.0F;
        float y = 0.0F;
        bool have_pos = false;
    };
    std::vector<McChild> mc_children;

    SubLayerNode mc_tree;

    double fps_measured = 0.0;
    uint64_t ifs_size_bytes = 0;
    uint64_t load_time_ms = 0;
    uint32_t stream_id = 0xFFFFFFFC;
    int frame_count = 0;
    bool label_playback_active = false;
};

enum class ExportPhase : std::uint8_t {
    Idle,
    Capturing,
    Encoding,
    Done,
    Failed,
};

struct ExportState {
    std::string output_path;
    std::string error;
    std::chrono::steady_clock::time_point start_time;
    int fps = 30;
    int quality = 60;
    int frames_captured = 0;
    int format = 0;
    float bg_r = 0.0F;
    float bg_g = 0.0F;
    float bg_b = 0.0F;
    ExportPhase phase = ExportPhase::Idle;
    bool bg_transparent = true;
    bool using_hardware = false;
};

class Telemetry {
public:
    [[nodiscard]] Status GetStatus() const;
    void SetStatus(Status s);

    [[nodiscard]] ExportState GetExport() const;
    void SetExport(ExportState e);

private:
    mutable std::mutex mu_;
    Status status_;
    ExportState export_;
};

}
