#pragma once

#include <array>
#include <cstdint>
#include <mutex>

namespace App {

struct CropRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

class LiveControls {
public:
    enum class RootLoopMode : std::uint8_t {
        Hold = 0,
        Force = 1,
    };

    struct LiveOverrides {
        int continuous_loop_mode = 0;
        int trim_frames = 0;
        int bg_color_index = -1;
        int mc_name_type = 0;
        bool filter_enabled = false;
        bool paused = false;
        bool show_mc_names = false;
    };

    struct LiveState {
        std::array<char, 32> conv_engine = {};
        uint64_t size_bytes = 0;
        uint64_t load_time_ms = 0;
        uint32_t stream_id = 0xFFFFFFFC;
        uint32_t cur_pos = 0;
        uint32_t total_length = 0;
        uint32_t flags0 = 0;
        uint32_t mc_cur = 0;
        uint32_t mc_total = 0;
        uint32_t mc_loop_count = 0;
        uint32_t mc_wrap_count = 0;
        uint32_t mc_w = 0;
        uint32_t mc_h = 0;
        uint32_t conv_ver = 0;
        uint32_t pkg_ver = 0;
        uint32_t afp_ver = 0;
        int frames_since_switch = 0;
        bool have_layer_info = false;
        bool master_complete = false;
        bool label_active = false;
        bool have_mc_playhead = false;
        bool have_file_info = false;
        bool filter_on = false;
    };

    static constexpr std::array<uint32_t, 5> kBgPresets = {0xFF808080U, 0x00000000U, 0xFFFF0000U,
                                                           0xFF00FF00U, 0xFF0000FFU};

    void GetRenderSize(int& w, int& h) const;
    void SetRenderSize(int w, int h);

    [[nodiscard]] int GetRenderFps() const;
    void SetRenderFps(int fps);

    [[nodiscard]] bool GetLoopMaster() const;
    void SetLoopMaster(bool on);

    [[nodiscard]] RootLoopMode GetRootLoopMode() const;
    void SetRootLoopMode(RootLoopMode m);

    [[nodiscard]] float GetMasterScale() const;
    void SetMasterScale(float s);

    [[nodiscard]] LiveOverrides GetLiveOverrides() const;
    void SetLiveOverrides(LiveOverrides o);

    [[nodiscard]] LiveState GetLiveState() const;
    void SetLiveState(const LiveState& s);

    [[nodiscard]] CropRect GetCropRect() const;
    void SetCropRect(CropRect r);

    [[nodiscard]] bool GetCropPickMode() const;
    void SetCropPickMode(bool on);

private:
    mutable std::mutex mu_;
    LiveState live_state_;
    LiveOverrides live_overrides_;
    CropRect crop_rect_;
    float master_scale_{1.0F};
    int render_w_{1920};
    int render_h_{1080};
    int render_fps_{120};
    RootLoopMode root_loop_mode_{RootLoopMode::Hold};
    bool loop_master_{false};
    bool crop_pick_mode_{false};
};

}
