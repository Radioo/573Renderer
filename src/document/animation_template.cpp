#include "document/animation_template.h"

#include "document/animation_strings.h"
#include "document/image_shape.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <array>
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

constexpr std::array<std::string_view, 2> kHelperExports{"aep_mask_dummy", "aeplibset"};

std::optional<uint16_t> DefinedId(const AfpAnimation::Tag& tag) {
    if (const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body)) return sprite->id;
    if (const auto* shape = std::get_if<AfpAnimation::Shape>(&tag.body)) return shape->id;
    if (const auto* image = std::get_if<AfpAnimation::Image>(&tag.body)) return image->id;
    return std::nullopt;
}

const AfpAnimation::Tag* Definition(const AfpAnimation::Container& root, uint16_t id) {
    const auto found = std::ranges::find_if(
        root.tags, [id](const AfpAnimation::Tag& tag) { return DefinedId(tag) == id; });
    return found == root.tags.end() ? nullptr : &*found;
}

std::set<uint16_t> HelperIds(const AfpAnimation::Animation& like) {
    std::set<uint16_t> kept;
    std::vector<uint16_t> pending;
    for (const AfpAnimation::Export& exported : like.exports) {
        const std::string name = StringText(like, exported.name);
        if (std::ranges::find(kHelperExports, name) == kHelperExports.end()) continue;
        pending.push_back(exported.tag);
    }
    while (!pending.empty()) {
        const uint16_t id = pending.back();
        pending.pop_back();
        const AfpAnimation::Tag* defined = Definition(like.root, id);
        if (defined == nullptr || !kept.insert(id).second) continue;
        const auto* sprite = std::get_if<AfpAnimation::Sprite>(&defined->body);
        if (sprite == nullptr) continue;
        for (const AfpAnimation::Tag& inner : sprite->container.tags) {
            const auto* placed = std::get_if<AfpAnimation::Placement>(&inner.body);
            if (placed != nullptr && placed->character) pending.push_back(*placed->character);
        }
    }
    return kept;
}

AfpAnimation::Sprite EmptySprite(uint16_t id, uint32_t frames) {
    AfpAnimation::Sprite sprite{.id = id, .container = {}};
    sprite.container.frames.assign(frames, AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    return sprite;
}

}

AnimationTemplate EmptyLike(const AfpAnimation::Animation& like, std::string_view name,
                            uint32_t frames) {
    const std::set<uint16_t> kept = HelperIds(like);
    AnimationTemplate made{.animation = like, .shapes = {}};
    AfpAnimation::Animation& animation = made.animation;
    animation.root = AfpAnimation::Container{};
    if (like.root.script_labels) animation.root.script_labels = std::vector<AfpAnimation::Label>{};
    for (const AfpAnimation::Tag& tag : like.root.tags) {
        const std::optional<uint16_t> id = DefinedId(tag);
        if (!id || !kept.contains(*id)) continue;
        animation.root.tags.push_back(tag);
        if (std::holds_alternative<AfpAnimation::Shape>(tag.body)) made.shapes.push_back(*id);
    }
    std::ranges::sort(made.shapes);
    const uint16_t self = NextCharacterId(animation).value_or(0);
    animation.root.tags.emplace_back(EmptySprite(self, frames));
    const auto defined = static_cast<uint32_t>(animation.root.tags.size());
    animation.root.frames.assign(frames, AfpAnimation::Frame{.first_tag = defined, .tag_count = 0});
    animation.root.frames.front() = AfpAnimation::Frame{.first_tag = 0, .tag_count = defined};

    animation.name = InternString(animation, name);
    std::erase_if(animation.exports, [&kept](const AfpAnimation::Export& exported) {
        return !kept.contains(exported.tag);
    });
    animation.exports.push_back(AfpAnimation::Export{.tag = self, .name = animation.name});
    std::ranges::sort(animation.exports, {}, [&animation](const AfpAnimation::Export& exported) {
        return StringText(animation, exported.name);
    });
    CompactStrings(animation);
    return made;
}

}
