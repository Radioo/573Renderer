#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "qpro_model.h"
#include "state/fifo_queue.h"
#include "state/boot_lifecycle.h"
#include "state/live_controls.h"
#include "state/ifs_catalog.h"
#include "state/telemetry.h"

namespace App {

struct Request {
    std::string ifs_path;
    std::string game_dir;
    std::string game_profile;
    std::string qpro_out_dir;
    std::string animation_name;
    std::string animation_label;
    std::string goto_label_name;
    std::string export_output_path;
    std::string export_dump_frames_dir;
    QproModel::PartSelection qpro_part_sel;
    QproModel::CategorySel qpro_parts;
    int render_width = 0;
    int render_height = 0;
    int qpro_fps = 60;
    int companion_index = -1;
    int seek_to_frame = 0;
    int export_fps = 30;
    int export_quality = 60;
    int export_keyframe_interval = 0;
    int export_max_frames = 0;
    int export_loop_count = 1;
    int export_blend_frames = 15;
    int export_format = 0;
    int export_width = 0;
    int export_height = 0;
    int export_crop_x = 0;
    int export_crop_y = 0;
    int export_crop_w = 0;
    int export_crop_h = 0;
    float export_bg_r = 0.0F;
    float export_bg_g = 0.0F;
    float export_bg_b = 0.0F;
    bool load_new_ifs = false;
    bool ifs_from_arc = false;
    bool set_game_dir = false;
    bool start_qpro_extract = false;
    bool qpro_hue_scope = true;
    bool start_qpro_scan = false;
    bool switch_animation = false;
    bool goto_label = false;
    bool seek_frame = false;
    bool set_paused = false;
    bool paused_value = false;
    bool toggle_companion = false;
    bool force_replay = false;
    bool start_export = false;
    bool export_blend_loop = false;
    bool export_bg_transparent = true;
    bool export_prefer_hardware = true;
    bool cancel_export = false;
};

class State {
public:
    std::optional<Request> TakeRequest();
    void PostRequest(Request r);

    IfsConfig& MutConfig(const std::string& filename);
    const IfsConfig* FindConfig(const std::string& filename) const;

    [[nodiscard]] std::vector<std::pair<std::string, bool>>
    GetSublayerOverrides(const std::string& filename) const;
    void SetSublayerOverride(const std::string& filename, const std::string& clip_name,
                             bool visible);

    [[nodiscard]] std::vector<std::string> GetSublayerExpanded() const;
    void SetSublayerExpanded(const std::string& path, bool expanded);

    using IfsEntry = IfsCatalog::IfsEntry;
    [[nodiscard]] std::vector<IfsEntry> ListAvailableIfs() const;
    void SetAvailableIfs(std::vector<IfsEntry> v);

    void SetIfsScanning(bool scanning);
    [[nodiscard]] bool IsIfsScanning() const;
    void WaitForIfsScan() const;
    void SetIfsScanStatus(std::string s);
    [[nodiscard]] std::string GetIfsScanStatus() const;

    [[nodiscard]] Status GetStatus() const;
    void SetStatus(Status s);

    [[nodiscard]] std::string ActiveIfs() const;
    void SetActiveIfs(std::string name);

    [[nodiscard]] std::string GameDir() const;
    void SetGameDir(std::string dir);

    void GetRenderSize(int& w, int& h) const;
    void SetRenderSize(int w, int h);

    [[nodiscard]] int GetRenderFps() const;
    void SetRenderFps(int fps);

    [[nodiscard]] std::string GetGameProfileSlug() const;
    void SetGameProfileSlug(std::string slug);

    [[nodiscard]] bool IsDdrMode() const;
    void SetIsDdrMode(bool on);

    [[nodiscard]] BootState GetBootState() const;
    void SetBootState(BootState s);

    [[nodiscard]] std::string GetBootError() const;
    void SetBootError(std::string msg);

    [[nodiscard]] bool GetLoopMaster() const;
    void SetLoopMaster(bool on);

    using RootLoopMode = LiveControls::RootLoopMode;
    [[nodiscard]] RootLoopMode GetRootLoopMode() const;
    void SetRootLoopMode(RootLoopMode m);

    [[nodiscard]] float GetMasterScale() const;
    void SetMasterScale(float s);

    using LiveOverrides = LiveControls::LiveOverrides;
    static constexpr std::array<uint32_t, 5> kBgPresets = LiveControls::kBgPresets;
    [[nodiscard]] LiveOverrides GetLiveOverrides() const;
    void SetLiveOverrides(LiveOverrides o);

    using LiveState = LiveControls::LiveState;
    [[nodiscard]] LiveState GetLiveState() const;
    void SetLiveState(const LiveState& s);

    [[nodiscard]] ExportState GetExport() const;
    void SetExport(ExportState e);

    [[nodiscard]] CropRect GetCropRect() const;
    void SetCropRect(CropRect r);
    [[nodiscard]] bool GetCropPickMode() const;
    void SetCropPickMode(bool on);

    static constexpr int kLoadMinHoldMs = BootLifecycle::kLoadMinHoldMs;
    [[nodiscard]] LoadProgress GetLoadProgress() const;
    void SetLoadProgress(LoadProgress p);

    void BeginLoad(std::string target);
    void UpdateLoadStage(std::string stage, float fraction = -1.0F);
    void EndLoad();
    void SetLoadDetail(std::string detail);
    void SetTexturesExpected(int n);
    void BumpTexturesLoaded();

    std::atomic<bool>& ShouldExit() { return should_exit_; }

private:
    mutable std::mutex mu_;
    FifoQueue<Request> pending_;
    IfsCatalog catalog_;
    BootLifecycle boot_;
    LiveControls live_;
    Telemetry telemetry_;
    std::atomic<bool> should_exit_{false};
};

State& Global();

bool SaveCurrentSettings();

}
