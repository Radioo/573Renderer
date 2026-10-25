#include "document/script_index.h"

#include "document/clip.h"
#include "document/script_source.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Document {

namespace {

constexpr std::size_t kLongestPreview = 64;

std::string FirstLine(const std::string& source) {
    const std::size_t stop = source.find('\n');
    std::string line = stop == std::string::npos ? source : source.substr(0, stop);
    if (line.size() <= kLongestPreview) return line;
    return line.substr(0, kLongestPreview - 1) + "…";
}

void Describe(const AfpAnimation::Animation& animation, const AfpAnimation::Bytecode& bytecode,
              ScriptEntry& entry) {
    const std::optional<std::string> source = ScriptSourceText(animation, bytecode);
    if (!source) {
        entry.preview = "cannot be read as source";
        entry.shape = ScriptShape::Unreadable;
        return;
    }
    entry.preview = FirstLine(*source);
    entry.shape = ScriptIsCalls(*source) ? ScriptShape::Call : ScriptShape::Instructions;
}

std::string NameOf(const ClipSummary& clip) {
    return clip.name.empty() ? std::string("Root") : clip.name;
}

void GatherClip(const AfpAnimation::Animation& animation, const ClipSummary& clip,
                std::vector<ScriptEntry>& out) {
    const AfpAnimation::Container* body = FindClip(animation, clip.id);
    if (body == nullptr) return;
    std::vector<ScriptEntry> owned;
    for (uint32_t frame = 0; frame < body->frames.size(); frame++) {
        const AfpAnimation::Frame& owner = body->frames[frame];
        for (uint32_t at = 0; at < owner.tag_count; at++) {
            const std::size_t index = owner.first_tag + at;
            if (index >= body->tags.size()) break;
            const AfpAnimation::Tag& tag = body->tags[index];
            if (const auto* action = std::get_if<AfpAnimation::Action>(&tag.body)) {
                ScriptEntry entry{.place = {.clip = clip.id, .frame = frame, .depth = std::nullopt},
                                  .clip_name = NameOf(clip),
                                  .preview = {},
                                  .shape = ScriptShape::Call};
                Describe(animation, action->bytecode, entry);
                out.push_back(std::move(entry));
                continue;
            }
            const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
            if (placement == nullptr || !placement->clip_actions) continue;
            if (placement->clip_actions->events.empty()) continue;
            ScriptEntry entry{.place = {.clip = clip.id, .frame = frame, .depth = placement->depth},
                              .clip_name = NameOf(clip),
                              .preview = {},
                              .shape = ScriptShape::Call};
            Describe(animation, placement->clip_actions->events.front().bytecode, entry);
            owned.push_back(std::move(entry));
        }
    }
    std::ranges::stable_sort(owned, [](const ScriptEntry& one, const ScriptEntry& other) {
        return one.place.depth < other.place.depth;
    });
    out.insert(out.end(), std::make_move_iterator(owned.begin()),
               std::make_move_iterator(owned.end()));
}

}

std::vector<ScriptEntry> ScriptsIn(const AfpAnimation::Animation& animation) {
    std::vector<ScriptEntry> out;
    for (const ClipSummary& clip : Clips(animation))
        GatherClip(animation, clip, out);
    return out;
}

std::vector<std::string> CallsIn(const AfpAnimation::Animation& animation) {
    std::vector<std::string> out;
    for (const ScriptEntry& entry : ScriptsIn(animation)) {
        const std::optional<AfpAnimation::Bytecode> code = ScriptAt(animation, entry.place);
        if (!code) continue;
        const std::optional<std::string> source = ScriptSourceText(animation, *code);
        if (!source) continue;
        for (const std::string& call : ScriptCallsUsed(*source)) {
            if (std::ranges::find(out, call) == out.end()) out.push_back(call);
        }
    }
    std::ranges::sort(out);
    return out;
}

std::optional<AfpAnimation::Bytecode> ScriptAt(const AfpAnimation::Animation& animation,
                                               const ScriptPlace& place) {
    const AfpAnimation::Container* body = FindClip(animation, place.clip);
    if (body == nullptr || place.frame >= body->frames.size()) return std::nullopt;
    const AfpAnimation::Frame& owner = body->frames[place.frame];
    for (uint32_t at = 0; at < owner.tag_count; at++) {
        const std::size_t index = owner.first_tag + at;
        if (index >= body->tags.size()) break;
        const AfpAnimation::Tag& tag = body->tags[index];
        if (!place.depth) {
            if (const auto* action = std::get_if<AfpAnimation::Action>(&tag.body))
                return action->bytecode;
            continue;
        }
        const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement == nullptr || placement->depth != *place.depth) continue;
        if (!placement->clip_actions || placement->clip_actions->events.empty()) continue;
        return placement->clip_actions->events.front().bytecode;
    }
    return std::nullopt;
}

}
