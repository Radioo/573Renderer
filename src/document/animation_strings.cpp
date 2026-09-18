#include "document/animation_strings.h"

#include "formats/afp_animation.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Document {

namespace {

using Visit = std::function<void(AfpAnimation::StringId&)>;

void VisitBytecode(AfpAnimation::Bytecode& bytecode, const Visit& visit) {
    if (!bytecode.strings) return;
    for (AfpAnimation::StringId& id : *bytecode.strings)
        visit(id);
}

void VisitLabels(std::vector<AfpAnimation::Label>& labels, const Visit& visit) {
    for (AfpAnimation::Label& label : labels)
        visit(label.name);
}

void VisitContainer(AfpAnimation::Container& container, const Visit& visit);

void VisitTag(AfpAnimation::Tag& tag, const Visit& visit) {
    if (auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body)) {
        VisitContainer(sprite->container, visit);
    } else if (auto* action = std::get_if<AfpAnimation::Action>(&tag.body)) {
        VisitBytecode(action->bytecode, visit);
    } else if (auto* image = std::get_if<AfpAnimation::Image>(&tag.body)) {
        visit(image->name);
    } else if (auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body)) {
        if (placement->name) visit(*placement->name);
        if (placement->class_name) visit(*placement->class_name);
        if (!placement->clip_actions) return;
        for (AfpAnimation::ClipEvent& event : placement->clip_actions->events)
            VisitBytecode(event.bytecode, visit);
    }
}

void VisitContainer(AfpAnimation::Container& container, const Visit& visit) {
    VisitLabels(container.labels, visit);
    if (container.script_labels) VisitLabels(*container.script_labels, visit);
    for (AfpAnimation::Tag& tag : container.tags)
        VisitTag(tag, visit);
}

void VisitStringIds(AfpAnimation::Animation& animation, const Visit& visit) {
    visit(animation.name);
    for (AfpAnimation::Export& exported : animation.exports)
        visit(exported.name);
    for (AfpAnimation::Import& imported : animation.imports) {
        visit(imported.movie);
        for (AfpAnimation::ImportedAsset& asset : imported.assets)
            visit(asset.name);
    }
    VisitContainer(animation.root, visit);
}

}

AfpAnimation::StringId InternString(AfpAnimation::Animation& animation, std::string_view text) {
    const auto found = std::ranges::find(animation.strings, text);
    if (found != animation.strings.end())
        return static_cast<AfpAnimation::StringId>(found - animation.strings.begin());
    animation.strings.emplace_back(text);
    return static_cast<AfpAnimation::StringId>(animation.strings.size() - 1);
}

void CompactStrings(AfpAnimation::Animation& animation) {
    std::set<AfpAnimation::StringId> used{0};
    VisitStringIds(animation, [&](AfpAnimation::StringId& id) { used.insert(id); });

    std::vector<std::string> kept;
    std::map<AfpAnimation::StringId, AfpAnimation::StringId> moved;
    for (const AfpAnimation::StringId id : used) {
        if (id >= animation.strings.size()) continue;
        moved.emplace(id, static_cast<AfpAnimation::StringId>(kept.size()));
        kept.push_back(animation.strings[id]);
    }
    if (kept.empty()) return;

    VisitStringIds(animation, [&](AfpAnimation::StringId& id) {
        const auto found = moved.find(id);
        if (found != moved.end()) id = found->second;
    });
    animation.strings = std::move(kept);
}

std::string FoldedName(std::string_view text) {
    std::string out(text);
    std::ranges::transform(out, out.begin(), [](char c) {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    });
    return out;
}

void InsertExport(AfpAnimation::Animation& animation, uint16_t tag, std::string_view name) {
    const AfpAnimation::StringId id = InternString(animation, name);
    const std::string key = FoldedName(name);
    const auto at = std::ranges::find_if(animation.exports, [&](const AfpAnimation::Export& other) {
        return FoldedName(StringText(animation, other.name)) > key;
    });
    animation.exports.insert(at, AfpAnimation::Export{.tag = tag, .name = id});
}

std::string StringText(const AfpAnimation::Animation& animation, AfpAnimation::StringId id) {
    if (id >= animation.strings.size()) return {};
    return animation.strings[id];
}

void CarryStrings(AfpAnimation::Tag& tag, const AfpAnimation::Animation& from,
                  AfpAnimation::Animation& to) {
    VisitTag(tag, [&](AfpAnimation::StringId& id) { id = InternString(to, StringText(from, id)); });
}

}
