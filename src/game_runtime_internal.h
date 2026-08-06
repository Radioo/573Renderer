#pragma once

#include "game_runtime.h"

#include <cstdint>
#include <string>
#include <vector>

struct AfpFuncs;

namespace Runtime {

class ModernRuntime : public IGameRuntime {
public:
    bool IsBooted() override;
    std::string ActiveClipName() override;
    bool HasRenderableScene(uint32_t modern_stream_id) override;
    void RenderFrame(float dt, uint32_t modern_stream_id, int frame_count) override;
    void ReprobeVariantSlots(uint32_t modern_stream_id) override;
    void ApplyContinuousLoop(uint32_t modern_stream_id, int continuous_mode) override;
    void ApplyMasterScale(uint32_t modern_stream_id) override;
    void ApplyVariantSlots(uint32_t modern_stream_id) override;
    void ApplySublayerOverrides(uint32_t modern_stream_id) override;
    void ForceReplayMaster() override;
    bool LoadScene(const std::string& mount_path, const std::string& ifs_path) override;
    void UnloadScene() override;
    void Shutdown() override;
    uint32_t ActiveClipId(uint32_t modern_stream_id) override;
    bool HaveActiveClip(uint32_t modern_stream_id) override;
    bool ReadPlayhead(uint32_t modern_stream_id, uint32_t* cur, uint32_t* total,
                      uint32_t* raw_loop_count) override;
    bool ReadSize(uint32_t modern_stream_id, uint32_t* w, uint32_t* h) override;
    bool ReadRawLayerInfo(uint32_t modern_stream_id, uint32_t* raw_cur, uint32_t* raw_total,
                          uint32_t* flags0) override;
    bool ReadComplete(const AfpFuncs& afp, uint32_t modern_stream_id) override;
    std::vector<Label> EnumerateLabels(const AfpFuncs& afp, uint32_t modern_stream_id) override;
    void SetPaused(const AfpFuncs& afp, bool paused) override;
    bool SeekFrame(const AfpFuncs& afp, int frame) override;
    bool GotoLabel(const AfpFuncs& afp, const std::string& name) override;
    void SwitchAnimation(const std::string& name, const std::string& label) override;
    RootRedrive MaybeRedriveRootLoop(uint32_t stream_id, int loop_cooldown, int frames_since_switch,
                                     int trim_frames) override;
    bool SetGlobalSpeed(const AfpFuncs& afp, float speed) override;
    bool SupportsLiveExtras() override;
    bool IsLegacyDdr() override;

private:
    uint32_t flag_dance_done_for_ = kModernNoStream;
    uint32_t master_scale_sid_ = kModernNoStream;
    float master_scale_last_ = -1.0F;
    int flag_dance_mode_seen_ = 0;
};

class DdrRuntime : public IGameRuntime {
public:
    bool IsBooted() override;
    std::string ActiveClipName() override;
    bool HasRenderableScene(uint32_t modern_stream_id) override;
    void RenderFrame(float dt, uint32_t modern_stream_id, int frame_count) override;
    void ReprobeVariantSlots(uint32_t modern_stream_id) override;
    void ApplyContinuousLoop(uint32_t modern_stream_id, int continuous_mode) override;
    void ApplyMasterScale(uint32_t modern_stream_id) override;
    void ApplyVariantSlots(uint32_t modern_stream_id) override;
    void ApplySublayerOverrides(uint32_t modern_stream_id) override;
    void ForceReplayMaster() override;
    bool LoadScene(const std::string& mount_path, const std::string& ifs_path) override;
    void UnloadScene() override;
    void Shutdown() override;
    uint32_t ActiveClipId(uint32_t modern_stream_id) override;
    bool HaveActiveClip(uint32_t modern_stream_id) override;
    bool ReadPlayhead(uint32_t modern_stream_id, uint32_t* cur, uint32_t* total,
                      uint32_t* raw_loop_count) override;
    bool ReadSize(uint32_t modern_stream_id, uint32_t* w, uint32_t* h) override;
    bool ReadRawLayerInfo(uint32_t modern_stream_id, uint32_t* raw_cur, uint32_t* raw_total,
                          uint32_t* flags0) override;
    bool ReadComplete(const AfpFuncs& afp, uint32_t modern_stream_id) override;
    std::vector<Label> EnumerateLabels(const AfpFuncs& afp, uint32_t modern_stream_id) override;
    void SetPaused(const AfpFuncs& afp, bool paused) override;
    bool SeekFrame(const AfpFuncs& afp, int frame) override;
    bool GotoLabel(const AfpFuncs& afp, const std::string& name) override;
    void SwitchAnimation(const std::string& name, const std::string& label) override;
    RootRedrive MaybeRedriveRootLoop(uint32_t stream_id, int loop_cooldown, int frames_since_switch,
                                     int trim_frames) override;
    bool SetGlobalSpeed(const AfpFuncs& afp, float speed) override;
    bool SupportsLiveExtras() override;
    bool IsLegacyDdr() override;
};

}
