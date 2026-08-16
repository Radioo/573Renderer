#include "editor/options_model.h"

#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Editor {

namespace Doc = Preset::Doc;

namespace {

constexpr std::array<std::string_view, 7> kModelFields = {
    "position", "rotation", "scale", "alpha", "anim_speed", "spin_per_frame", "blend_mode"};

constexpr std::array<std::string_view, 6> kSpriteFields = {"x",     "y",     "alpha",
                                                           "scale", "blend", "priority"};

constexpr std::array<std::string_view, 6> kCameraFields = {"eye",   "at",     "up",
                                                           "fov_y", "near_z", "far_z"};

std::string Trimmed(double value) {
    std::array<char, 32> buffer = {};
    (void)snprintf(buffer.data(), buffer.size(), "%g", value);
    return buffer.data();
}

void PushUnique(std::vector<std::string>& list, std::string value) {
    if (std::ranges::find(list, value) != list.end()) return;
    list.push_back(std::move(value));
}

std::string ScopeName(const std::string& id, std::string_view prefix) {
    if (!id.starts_with(prefix)) return {};
    const std::size_t close = id.find(']');
    if (close == std::string::npos) return {};
    return id.substr(prefix.size(), close - prefix.size());
}

bool LabelTaken(const Doc::OptionSpec& option, int choice, const std::string& label) {
    for (std::size_t i = 0; i < option.choices.size(); i++) {
        if (std::cmp_equal(i, choice)) continue;
        if (option.choices[i].label == label) return true;
    }
    return false;
}

}

std::string TransitionSummary(const Doc::Transition& transition) {
    if (transition.frames <= 0) return "instant";
    const std::string out = std::to_string(transition.frames) + " f at step " +
                            std::to_string(std::max(0, transition.step));
    if (transition.spin_kick == 0.0) return out + ", no kick";
    return out + ", kick " + Trimmed(transition.spin_kick);
}

int TransitionSpanFrames(const Doc::Transition& transition) {
    if (transition.frames <= 0) return 0;
    if (transition.step <= 0) return transition.frames;
    return (transition.frames + transition.step - 1) / transition.step;
}

std::vector<std::string> MovedTargets(const Doc::OptionSpec& option, int from, int to) {
    std::vector<std::string> out;
    for (const int side : {from, to}) {
        if (side < 0 || std::cmp_greater_equal(side, option.choices.size())) continue;
        for (const Doc::ChoiceValue& value : option.choices[(std::size_t)side].values)
            PushUnique(out, value.id);
    }
    return out;
}

std::vector<std::string> TrackIdsForTargets(const Doc::Document& document,
                                            const std::vector<std::string>& targets) {
    std::vector<std::string> out;
    for (const std::string& target : targets) {
        const std::string model = ScopeName(target, "model[");
        const std::string sprite = ScopeName(target, "sprite[");
        const bool camera = target.starts_with("camera.");
        for (const Doc::Track& track : document.tracks) {
            const bool hit =
                (track.kind == Doc::TrackKind::Model && track.target == model && !model.empty()) ||
                (track.kind == Doc::TrackKind::Sprite && track.target == sprite &&
                 !sprite.empty()) ||
                (track.kind == Doc::TrackKind::Camera && camera);
            if (hit) PushUnique(out, track.id);
        }
    }
    if (out.empty()) out.emplace_back(kOptionBandRow);
    return out;
}

std::vector<std::string> ChoiceValueTargets(const Doc::Document& document) {
    std::vector<std::string> out;
    for (const Doc::Track& track : document.tracks) {
        if (track.kind == Doc::TrackKind::Model) {
            for (const std::string_view field : kModelFields)
                PushUnique(out, "model[" + track.target + "]." + std::string(field));
        }
        if (track.kind != Doc::TrackKind::Sprite) continue;
        for (const std::string_view field : kSpriteFields)
            PushUnique(out, "sprite[" + track.target + "]." + std::string(field));
    }
    for (const std::string_view field : kCameraFields)
        PushUnique(out, "camera." + std::string(field));
    PushUnique(out, "sprite_split_priority");
    return out;
}

int AddOption(Doc::Document& document) {
    std::string id = "option";
    for (int suffix = 2; suffix < 1000; suffix++) {
        const bool taken = std::ranges::any_of(
            document.options, [&id](const Doc::OptionSpec& option) { return option.id == id; });
        if (!taken) break;
        id = "option_" + std::to_string(suffix);
    }
    document.options.push_back(Doc::OptionSpec{.id = id,
                                               .label = "New option",
                                               .default_choice = 0,
                                               .choices = {Doc::ChoiceSpec{.label = "Choice 1"}}});
    return (int)document.options.size() - 1;
}

bool RenameChoice(Doc::Document& document, int option, int choice, const std::string& label) {
    if (option < 0 || std::cmp_greater_equal(option, document.options.size())) return false;
    Doc::OptionSpec& spec = document.options[(std::size_t)option];
    if (choice < 0 || std::cmp_greater_equal(choice, spec.choices.size())) return false;
    if (label.empty() || LabelTaken(spec, choice, label)) return false;

    const std::string previous = spec.choices[(std::size_t)choice].label;
    if (previous == label) return false;
    spec.choices[(std::size_t)choice].label = label;
    for (Doc::Track& track : document.tracks) {
        for (Doc::Clip& clip : track.clips) {
            if (!clip.when.has_value() || clip.when->option != spec.id) continue;
            for (std::string& named : clip.when->choices) {
                if (named == previous) named = label;
            }
        }
    }
    return true;
}

bool MoveChoice(Doc::Document& document, int option, int choice, int delta) {
    if (option < 0 || std::cmp_greater_equal(option, document.options.size())) return false;
    Doc::OptionSpec& spec = document.options[(std::size_t)option];
    const int to = choice + delta;
    if (choice < 0 || std::cmp_greater_equal(choice, spec.choices.size())) return false;
    if (to < 0 || std::cmp_greater_equal(to, spec.choices.size())) return false;

    std::swap(spec.choices[(std::size_t)choice], spec.choices[(std::size_t)to]);
    if (spec.default_choice == choice) {
        spec.default_choice = to;
    } else if (spec.default_choice == to) {
        spec.default_choice = choice;
    }
    return true;
}

}
