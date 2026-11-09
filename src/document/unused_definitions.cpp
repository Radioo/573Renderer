#include "document/unused_definitions.h"

#include "document/document.h"
#include "document/tags.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Document {

namespace {

Support::Expected<void, std::string> CheckFlat(const AfpAnimation::Container& root) {
    for (const AfpAnimation::Tag& tag : root.tags) {
        const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body);
        if (sprite == nullptr) continue;
        const bool nested =
            std::ranges::any_of(sprite->container.tags, [](const AfpAnimation::Tag& inner) {
                return std::holds_alternative<AfpAnimation::Sprite>(inner.body);
            });
        if (nested) {
            return Support::Unexpected("sprite " + std::to_string(sprite->id) +
                                       " defines another sprite inside itself, so what it uses "
                                       "cannot be followed");
        }
    }
    return {};
}

const AfpAnimation::Sprite* SpriteWith(const AfpAnimation::Animation& animation, uint16_t id) {
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body);
        if (sprite != nullptr && sprite->id == id) return sprite;
    }
    return nullptr;
}

void ReachFrom(const AfpAnimation::Animation& animation, const AfpAnimation::Container& clip,
               std::set<uint16_t>& used);

void Reach(const AfpAnimation::Animation& animation, uint16_t id, std::set<uint16_t>& used) {
    if (!used.insert(id).second) return;
    const AfpAnimation::Sprite* sprite = SpriteWith(animation, id);
    if (sprite != nullptr) ReachFrom(animation, sprite->container, used);
}

void ReachFrom(const AfpAnimation::Animation& animation, const AfpAnimation::Container& clip,
               std::set<uint16_t>& used) {
    for (const AfpAnimation::Tag& tag : clip.tags) {
        const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement == nullptr) continue;
        if (placement->character) Reach(animation, *placement->character, used);
        if (placement->grid_controller) Reach(animation, placement->grid_controller->tag, used);
    }
}

std::set<uint16_t> Used(const AfpAnimation::Animation& animation) {
    std::set<uint16_t> used;
    ReachFrom(animation, animation.root, used);
    for (const AfpAnimation::Export& exported : animation.exports)
        Reach(animation, exported.tag, used);
    return used;
}

std::optional<uint16_t> RemovableId(const AfpAnimation::Tag& tag) {
    if (const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body)) return sprite->id;
    if (const auto* shape = std::get_if<AfpAnimation::Shape>(&tag.body)) return shape->id;
    return std::nullopt;
}

}

Support::Expected<std::vector<uint16_t>, std::string>
RemoveUnusedDefinitions(File& file, std::string_view animation_path) {
    auto animation = file.ReadAnimation(animation_path);
    if (!animation) return Support::Unexpected(animation.error());
    auto readable = CheckFlat(animation->root);
    if (!readable) return Support::Unexpected(readable.error());

    const std::set<uint16_t> used = Used(*animation);
    std::vector<uint16_t> removed;
    std::vector<uint16_t> shapes;
    for (std::size_t at = animation->root.tags.size(); at > 0; at--) {
        const AfpAnimation::Tag& tag = animation->root.tags[at - 1];
        const std::optional<uint16_t> id = RemovableId(tag);
        if (!id || used.contains(*id)) continue;
        if (std::holds_alternative<AfpAnimation::Shape>(tag.body)) shapes.push_back(*id);
        removed.insert(removed.begin(), *id);
        EraseTag(animation->root, at - 1);
    }
    if (removed.empty()) return removed;

    File edited = file;
    auto written = edited.WriteAnimation(animation_path, *animation);
    if (!written) return Support::Unexpected(written.error());
    for (const uint16_t id : shapes) {
        auto gone = edited.RemoveShapeFile(animation_path, id);
        if (!gone) return Support::Unexpected(gone.error());
    }
    file = std::move(edited);
    return removed;
}

}
