#include "editor/timeline_lanes.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"

#include <algorithm>
#include <vector>

namespace Editor {

namespace Doc = Preset::Doc;

namespace {

bool IsModifier(const Doc::Clip& clip) {
    return Doc::TraitsFor(Doc::TypeOf(clip.command)).family == Doc::Family::None;
}

bool HasPrimary(const Doc::Track& track) {
    return std::ranges::any_of(track.clips,
                               [](const Doc::Clip& clip) { return !IsModifier(clip); });
}

std::vector<Doc::CommandType> ModifierTypes(const Doc::Track& track) {
    std::vector<Doc::CommandType> types;
    for (const Doc::Clip& clip : track.clips) {
        if (!IsModifier(clip)) continue;
        const Doc::CommandType type = Doc::TypeOf(clip.command);
        if (std::ranges::find(types, type) != types.end()) continue;
        types.push_back(type);
    }
    return types;
}

}

int LaneCount(const Doc::Track& track) {
    if (!HasPrimary(track)) return 1;
    return 1 + (int)ModifierTypes(track).size();
}

int LaneOf(const Doc::Track& track, const Doc::Clip& clip) {
    if (!IsModifier(clip) || !HasPrimary(track)) return 0;
    const std::vector<Doc::CommandType> types = ModifierTypes(track);
    const auto it = std::ranges::find(types, Doc::TypeOf(clip.command));
    if (it == types.end()) return 1;
    return 1 + (int)(it - types.begin());
}

}
