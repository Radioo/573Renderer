#include "document/sprite_exports.h"

#include "document/animation_strings.h"
#include "document/document.h"
#include "document/outline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Document {

namespace {

constexpr std::array<std::string_view, 2> kHelperExports{"aep_mask_dummy", "aeplibset"};

bool Defines(const AfpAnimation::Animation& animation, uint16_t sprite) {
    return std::ranges::any_of(animation.root.tags, [sprite](const AfpAnimation::Tag& tag) {
        const auto* defined = std::get_if<AfpAnimation::Sprite>(&tag.body);
        return defined != nullptr && defined->id == sprite;
    });
}

bool Reserved(const AfpAnimation::Animation& animation, std::string_view name) {
    const std::string folded = FoldedName(name);
    if (folded == FoldedName(StringText(animation, animation.name))) return true;
    return std::ranges::any_of(kHelperExports,
                               [&folded](std::string_view helper) { return helper == folded; });
}

bool Plain(std::string_view name) {
    return std::ranges::all_of(name, [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
               c == '_';
    });
}

void CollectAnimations(const std::vector<Node>& nodes, std::vector<const Node*>& out) {
    for (const Node& node : nodes) {
        if (node.role == Role::Animation) out.push_back(&node);
        CollectAnimations(node.children, out);
    }
}

Support::Expected<void, std::string> CheckNotImported(const File& file,
                                                      std::string_view animation_path,
                                                      const std::string& movie,
                                                      const std::string& name) {
    std::vector<const Node*> animations;
    CollectAnimations(file.Nodes(), animations);
    for (const Node* node : animations) {
        if (node->path == animation_path) continue;
        const auto other = file.ReadAnimation(node->path);
        if (!other) continue;
        for (const AfpAnimation::Import& imported : other->imports) {
            if (FoldedName(StringText(*other, imported.movie)) != FoldedName(movie)) continue;
            for (const AfpAnimation::ImportedAsset& asset : imported.assets) {
                if (FoldedName(StringText(*other, asset.name)) != FoldedName(name)) continue;
                std::string message = node->name;
                message += " imports ";
                message += name;
                message += " from ";
                message += movie;
                message += ", so that name has to stay";
                return Support::Unexpected(std::move(message));
            }
        }
    }
    return {};
}

Support::Expected<void, std::string> CheckNewName(const AfpAnimation::Animation& animation,
                                                  uint16_t sprite, std::string_view name) {
    if (name.empty()) return {};
    if (!Plain(name)) {
        return Support::Unexpected(
            std::string("an export name is made of ASCII letters, digits and underscores"));
    }
    if (Reserved(animation, name)) {
        return Support::Unexpected(std::string(name) +
                                   " is a name the animation's own lookups need");
    }
    const std::string folded = FoldedName(name);
    const bool taken =
        std::ranges::any_of(animation.exports, [&](const AfpAnimation::Export& exported) {
            return exported.tag != sprite &&
                   FoldedName(StringText(animation, exported.name)) == folded;
        });
    if (taken) {
        return Support::Unexpected("another symbol is already exported as " + std::string(name) +
                                   ", and the game compares export names ignoring case");
    }
    return {};
}

}

std::string SpriteExportName(const File& file, std::string_view animation_path, uint16_t sprite) {
    const auto animation = file.ReadAnimation(animation_path);
    if (!animation) return {};
    const auto exported = std::ranges::find(animation->exports, sprite, &AfpAnimation::Export::tag);
    if (exported == animation->exports.end()) return {};
    return StringText(*animation, exported->name);
}

Support::Expected<void, std::string> NameSpriteExport(File& file, std::string_view animation_path,
                                                      uint16_t sprite, std::string_view name) {
    auto animation = file.ReadAnimation(animation_path);
    if (!animation) return Support::Unexpected(animation.error());
    if (!Defines(*animation, sprite)) {
        return Support::Unexpected("the animation defines no sprite " + std::to_string(sprite));
    }
    const auto count = std::ranges::count(animation->exports, sprite, &AfpAnimation::Export::tag);
    if (count > 1) {
        return Support::Unexpected(std::string(
            "that sprite is exported under more than one name, which the editor does not change"));
    }
    const auto current = std::ranges::find(animation->exports, sprite, &AfpAnimation::Export::tag);
    if (current != animation->exports.end()) {
        const std::string old = StringText(*animation, current->name);
        if (FoldedName(old) == FoldedName(name)) {
            if (old == name) return {};
        } else if (Reserved(*animation, old)) {
            return Support::Unexpected(old + " is a name the animation's own lookups need");
        } else {
            auto imported = CheckNotImported(file, animation_path,
                                             StringText(*animation, animation->name), old);
            if (!imported) return Support::Unexpected(imported.error());
        }
    }
    auto checked = CheckNewName(*animation, sprite, name);
    if (!checked) return Support::Unexpected(checked.error());

    if (current != animation->exports.end()) animation->exports.erase(current);
    if (!name.empty()) InsertExport(*animation, sprite, name);
    CompactStrings(*animation);
    return file.WriteAnimation(animation_path, *animation);
}

}
