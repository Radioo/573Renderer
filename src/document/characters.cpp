#include "document/characters.h"

#include "document/animation_strings.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace Document {

namespace {

std::string Named(const std::string& kind, uint16_t id, const std::string& name) {
    std::string label = kind + " " + std::to_string(id);
    if (!name.empty()) label += ": " + name;
    return label;
}

std::string ExportName(const AfpAnimation::Animation& animation, uint16_t id) {
    const auto exported = std::ranges::find(animation.exports, id, &AfpAnimation::Export::tag);
    if (exported == animation.exports.end()) return {};
    return StringText(animation, exported->name);
}

std::string ShapeName(const AfpAnimation::Animation& animation, uint16_t id,
                      const std::map<uint16_t, std::string>& shape_images) {
    std::string name = ExportName(animation, id);
    if (!name.empty()) return name;
    const auto image = shape_images.find(id);
    return image == shape_images.end() ? std::string() : image->second;
}

}

std::vector<CharacterSummary> Characters(const AfpAnimation::Animation& animation,
                                         const std::map<uint16_t, std::string>& shape_images) {
    std::vector<CharacterSummary> out;
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        if (const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body)) {
            out.push_back(CharacterSummary{
                .id = sprite->id,
                .kind = CharacterKind::Sprite,
                .label = Named("Sprite", sprite->id, ExportName(animation, sprite->id))});
        } else if (const auto* image = std::get_if<AfpAnimation::Image>(&tag.body)) {
            out.push_back(CharacterSummary{
                .id = image->id,
                .kind = CharacterKind::Image,
                .label = Named("Image", image->id, StringText(animation, image->name))});
        } else if (const auto* shape = std::get_if<AfpAnimation::Shape>(&tag.body)) {
            out.push_back(CharacterSummary{
                .id = shape->id,
                .kind = CharacterKind::Shape,
                .label = Named("Shape", shape->id, ShapeName(animation, shape->id, shape_images))});
        }
    }
    for (const AfpAnimation::Import& imported : animation.imports) {
        for (const AfpAnimation::ImportedAsset& asset : imported.assets) {
            out.push_back(
                CharacterSummary{.id = asset.tag,
                                 .kind = CharacterKind::Imported,
                                 .label = Named("Imported", asset.tag,
                                                StringText(animation, asset.name) + " from " +
                                                    StringText(animation, imported.movie))});
        }
    }
    std::ranges::stable_sort(out, {}, &CharacterSummary::id);
    return out;
}

}
