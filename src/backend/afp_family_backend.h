#pragma once

#include "backend/backend.h"
#include "game_runtime.h"
#include "loop/submonitor_cycler.h"
#include "mc_control.h"

#include <any>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace AfpProfiles {
struct AfpConfig;
}

namespace Backend {

class AfpFamilyBackend : public IBackend {
public:
    bool Boot(const BootEnv& env) final;
    void Shutdown() final;
    void StartContentScan() final;
    bool LoadContent(const std::string& path, bool from_arc) final;
    void UnloadContent() final;
    void AdvanceFrame(float dt, int frame_count, bool exporting) final;
    void RenderScene(float dt, int frame_count) final;
    void FillAutopilotInputs(Loop::AutopilotInputs& in) final;
    void BindSubmonitor() final;
    bool HandleCommand(const std::any& payload) final;

protected:
    virtual bool BootEngine(const BootEnv& env) = 0;

    const GameProfile::Profile* profile_ = nullptr;
    const AfpProfiles::AfpConfig* cfg_ = nullptr;
    const Cli::Options* cli_ = nullptr;
    std::string game_dir_;

private:
    uint32_t ApplyLoopHousekeeping(uint32_t stream_id, bool exporting);
    void TickSubmonitorCyclers(uint32_t stream_id);
    void BindSubmonitorFade(uint32_t sid, const std::vector<McControl::ImageSlot>& slots,
                            int decoded);
    void BindSubmonitorSlideshow(uint32_t sid, const std::vector<McControl::ImageSlot>& slots,
                                 int decoded);

    std::vector<McControl::ImageSlot> sm_slots_;
    Loop::DissolveCycler sm_dissolve_{0};
    Loop::FadeCycler sm_fade_{0, 0};
    int sm_base_mc_ = -1;
    int sm_overlay_mc_ = -1;
    uint32_t loop_last_sid_ = Runtime::kModernNoStream;
    int loop_cooldown_ = 0;
    int frames_since_switch_ = 0;
};

std::unique_ptr<IBackend> MakeAfpModernBackend();

std::unique_ptr<IBackend> MakeAfpDdrBackend();

}
