#include "preset_legacy_view.h"

#include "preset_push_text.h"

#include "preset/eval/eval_emit.h"
#include "preset/eval/eval_push.h"
#include "preset/preset_effective.h"
#include "preset/scene_preset.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace PresetLegacy {

namespace {

using Preset::Eval::Push;
using Preset::Eval::PushCall;

bool Moves(const Preset::ModelMotion& motion) {
    return motion.orbit_radius > 0.0F || motion.spin_kick > 0.0F ||
           motion.spin_per_frame != std::array<float, 3>{0.0F, 0.0F, 0.0F};
}

bool ResolvedAlpha(const std::vector<Push>& pushes, const std::string& model, float& out) {
    bool found = false;
    for (const Push& push : pushes) {
        if (push.call != PushCall::SetModelAlpha || push.name != model) continue;
        out = push.value;
        found = true;
    }
    return found;
}

std::vector<std::string> HiddenMovers(const Preset::Scene& scene,
                                      std::span<const Preset::ParamOverride> params) {
    const Preset::Materialized mat = Preset::Materialize(scene, params, {});
    std::vector<std::string> out;
    for (const Preset::ModelState& model : mat.effective.models) {
        if (model.visible || !Moves(model.motion)) continue;
        out.push_back(model.model);
    }
    return out;
}

}

Adapter::Adapter(const Preset::Scene& scene) : scene_(scene) {
    if (scene.phases.empty()) {
        hidden_movers_.push_back(HiddenMovers(scene, {}));
    } else {
        for (const Preset::Phase& phase : scene.phases)
            hidden_movers_.push_back(HiddenMovers(scene, phase.params));
    }
    if (!scene.models.empty()) lead_ = std::string(scene.models.front().model);
}

int Adapter::PhaseAt(int frame) const {
    if (scene_.phases.empty()) return 0;
    int found = 0;
    for (std::size_t i = 0; i < scene_.phases.size(); i++) {
        if (scene_.phases[i].start_frame <= frame) found = (int)i;
    }
    return found;
}

bool Adapter::Excluded(int record_frame) const {
    return !hidden_movers_[(std::size_t)PhaseAt(record_frame + 1)].empty();
}

std::vector<std::string> Adapter::StripHidden(const std::vector<std::string>& calls,
                                              int record_frame) const {
    const std::vector<std::string>& hidden = hidden_movers_[(std::size_t)PhaseAt(record_frame + 1)];
    std::vector<std::string> out;
    for (const std::string& call : calls) {
        bool drop = false;
        for (const std::string& name : hidden)
            drop = drop || call.starts_with("Scene3dHost::SetModelTransform('" + name + "'");
        if (!drop) out.push_back(call);
    }
    return out;
}

FrameCalls Adapter::Filter(const std::vector<Push>& pushes, int record_frame) const {
    std::vector<Push> kept;
    kept.reserve(pushes.size());
    for (const Push& push : pushes) {
        if (!push.legacy) continue;
        if (push.call == PushCall::SetModelTransform && !push.model_moves) continue;
        kept.push_back(push);
    }

    FrameCalls out;
    const Preset::Countdown& countdown = scene_.countdown;
    const int at = record_frame + 1;
    if (countdown.start_frames > 0 && countdown.ramp_below > 1 && !lead_.empty()) {
        const int remaining = std::max(0, countdown.start_frames - at);
        if (remaining < countdown.ramp_below) {
            const auto elapsed = (float)(countdown.ramp_below - remaining);
            const float speed = countdown.speed_base + (elapsed * countdown.speed_per_frame);
            for (std::size_t i = kept.size(); i > 0; i--) {
                Push& push = kept[i - 1];
                if (push.call != PushCall::SetModelSpeed) continue;
                out.countdown_drift = std::max(out.countdown_drift, std::abs(push.value - speed));
                push.value = speed;
                break;
            }
            if (countdown.fade_per_frame > 0.0F) {
                const float faded = std::clamp(
                    countdown.fade_from - (elapsed * countdown.fade_per_frame), 0.0F, 1.0F);
                float resolved = faded;
                if (ResolvedAlpha(pushes, lead_, resolved))
                    out.countdown_drift = std::max(out.countdown_drift, std::abs(resolved - faded));
                kept.push_back(
                    Preset::Eval::ScalarPush(PushCall::SetModelAlpha, lead_, faded, true));
            }
        }
    }

    out.calls.reserve(kept.size());
    for (const Push& push : kept)
        out.calls.push_back(PresetPushText::Format(push, true));
    return out;
}

}
