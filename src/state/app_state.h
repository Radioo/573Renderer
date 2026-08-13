#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "state/commands.h"
#include "state/fifo_queue.h"
#include "state/boot_lifecycle.h"
#include "state/live_controls.h"
#include "state/ifs_catalog.h"
#include "state/telemetry.h"

namespace App {

class State {
public:
    std::optional<Command> TakeCommand();
    void PostCommand(Command c);

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

    [[nodiscard]] bool GetStretchWide() const;
    void SetStretchWide(bool on);

    [[nodiscard]] Stretch::Filter GetStretchFilter() const;
    void SetStretchFilter(Stretch::Filter filter);

    [[nodiscard]] int GetRenderFps() const;
    void SetRenderFps(int fps);

    [[nodiscard]] std::string GetGameProfileSlug() const;
    void SetGameProfileSlug(std::string slug);

    [[nodiscard]] std::string ActiveBackendId() const;
    void SetActiveBackendId(std::string id);

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
    void MutateLiveOverrides(const std::function<void(LiveOverrides&)>& fn);
    void ApplyLiveOverridesDelta(const LiveOverrides& before, const LiveOverrides& after);

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
    FifoQueue<Command> pending_;
    IfsCatalog catalog_;
    BootLifecycle boot_;
    LiveControls live_;
    Telemetry telemetry_;
    std::atomic<bool> should_exit_{false};
};

State& Global();

bool SaveCurrentSettings();

}
