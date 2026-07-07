#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct AfpFuncs;

namespace Runtime {

struct Label {
    std::string name;
    int frame = 0;
};

struct RootRedrive {
    bool replayed = false;
    bool reset_flag_dance = false;
    uint32_t new_stream_id = 0;
};

constexpr uint32_t kModernNoStream = 0xFFFFFFFC;

class IGameRuntime {
public:
    IGameRuntime() = default;
    virtual ~IGameRuntime() = default;
    IGameRuntime(const IGameRuntime&) = delete;
    IGameRuntime& operator=(const IGameRuntime&) = delete;
    IGameRuntime(IGameRuntime&&) = delete;
    IGameRuntime& operator=(IGameRuntime&&) = delete;

    virtual bool IsBooted() = 0;

    virtual std::string ActiveClipName() = 0;

    virtual bool HasRenderableScene(uint32_t modern_stream_id) = 0;

    virtual void RenderFrame(float dt) = 0;

    virtual void ReprobeVariantSlots(uint32_t modern_stream_id) = 0;

    virtual bool LoadScene(const std::string& mount_path, const std::string& ifs_path) = 0;

    virtual void UnloadScene() = 0;

    virtual void Shutdown() = 0;

    virtual uint32_t ActiveClipId(uint32_t modern_stream_id) = 0;

    virtual bool HaveActiveClip(uint32_t modern_stream_id) = 0;

    virtual bool ReadPlayhead(uint32_t modern_stream_id, uint32_t* cur, uint32_t* total,
                              uint32_t* raw_loop_count) = 0;

    virtual bool ReadSize(uint32_t modern_stream_id, uint32_t* w, uint32_t* h) = 0;

    virtual bool ReadRawLayerInfo(uint32_t modern_stream_id, uint32_t* raw_cur, uint32_t* raw_total,
                                  uint32_t* flags0) = 0;

    virtual bool ReadComplete(const AfpFuncs& afp, uint32_t modern_stream_id) = 0;

    virtual std::vector<Label> EnumerateLabels(const AfpFuncs& afp, uint32_t modern_stream_id) = 0;

    virtual void SetPaused(const AfpFuncs& afp, bool paused) = 0;

    virtual bool SeekFrame(const AfpFuncs& afp, int frame) = 0;

    virtual bool GotoLabel(const AfpFuncs& afp, const std::string& name) = 0;

    virtual void SwitchAnimation(const std::string& name, const std::string& label) = 0;

    virtual RootRedrive MaybeRedriveRootLoop(uint32_t stream_id, int loop_cooldown,
                                             int frames_since_switch, int trim_frames) = 0;

    virtual bool SetGlobalSpeed(const AfpFuncs& afp, float speed) = 0;

    virtual bool SupportsLiveExtras() = 0;

    virtual bool IsLegacyDdr() = 0;
};

void SelectRuntime(bool legacy_ddr);

IGameRuntime& Active();

}
