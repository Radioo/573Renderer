#include "preset_legacy_view.h"

#include "preset_push_text.h"

#include "preset/eval/eval_emit.h"
#include "preset/eval/eval_push.h"

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace PresetLegacy {

namespace {

using Preset::Eval::Push;
using Preset::Eval::PushCall;

bool ResolvedAlpha(const std::vector<Push>& pushes, const std::string& model, float& out) {
    bool found = false;
    for (const Push& push : pushes) {
        if (push.call != PushCall::SetModelAlpha || push.name != model) continue;
        out = push.value;
        found = true;
    }
    return found;
}

float ReadFloat(const nlohmann::json& node, const std::string& key) {
    return node.contains(key) ? node[key].get<float>() : 0.0F;
}

}

std::map<std::string, Compat> LoadCompat(const std::string& text, std::string& err) {
    std::map<std::string, Compat> out;
    const nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        err = "legacy compat is not a JSON object";
        return out;
    }
    for (const auto& [id, node] : root.items()) {
        Compat compat;
        compat.lead = node["lead"].get<std::string>();
        const nlohmann::json& countdown = node["countdown"];
        compat.countdown.start_frames = countdown["start_frames"].get<int>();
        compat.countdown.ramp_below = countdown["ramp_below"].get<int>();
        compat.countdown.speed_base = ReadFloat(countdown, "speed_base");
        compat.countdown.speed_per_frame = ReadFloat(countdown, "speed_per_frame");
        compat.countdown.fade_from = ReadFloat(countdown, "fade_from");
        compat.countdown.fade_per_frame = ReadFloat(countdown, "fade_per_frame");
        for (const auto& start : node["phase_starts"])
            compat.phase_starts.push_back(start.get<int>());
        for (const auto& phase : node["hidden_movers"]) {
            std::vector<std::string> models;
            for (const auto& model : phase)
                models.push_back(model.get<std::string>());
            compat.hidden_movers.push_back(std::move(models));
        }
        if (compat.phase_starts.size() != compat.hidden_movers.size()) {
            err = id + ": phase_starts and hidden_movers disagree";
            return {};
        }
        out[id] = std::move(compat);
    }
    if (out.empty()) err = "legacy compat holds no presets";
    return out;
}

Adapter::Adapter(Compat compat) : compat_(std::move(compat)) {}

int Adapter::PhaseAt(int frame) const {
    int found = 0;
    for (std::size_t i = 0; i < compat_.phase_starts.size(); i++) {
        if (compat_.phase_starts[i] <= frame) found = (int)i;
    }
    return found;
}

bool Adapter::Excluded(int record_frame) const {
    return !compat_.hidden_movers[(std::size_t)PhaseAt(record_frame + 1)].empty();
}

std::vector<std::string> Adapter::StripHidden(const std::vector<std::string>& calls,
                                              int record_frame) const {
    const std::vector<std::string>& hidden =
        compat_.hidden_movers[(std::size_t)PhaseAt(record_frame + 1)];
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
    const Countdown& countdown = compat_.countdown;
    const int at = record_frame + 1;
    if (countdown.start_frames > 0 && countdown.ramp_below > 1 && !compat_.lead.empty()) {
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
                if (ResolvedAlpha(pushes, compat_.lead, resolved))
                    out.countdown_drift = std::max(out.countdown_drift, std::abs(resolved - faded));
                kept.push_back(
                    Preset::Eval::ScalarPush(PushCall::SetModelAlpha, compat_.lead, faded, true));
            }
        }
    }

    out.calls.reserve(kept.size());
    for (const Push& push : kept)
        out.calls.push_back(PresetPushText::Format(push, true));
    return out;
}

}
