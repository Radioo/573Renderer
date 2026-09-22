#include "document/span_transplant.h"

#include "document/animation_strings.h"
#include "document/clip.h"
#include "document/document.h"
#include "document/image_shape.h"
#include "document/span_clipboard.h"
#include "document/tags.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Document {

namespace {

const AfpAnimation::Tag* Definition(const AfpAnimation::Animation& animation, uint16_t id) {
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        if (const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body)) {
            if (sprite->id == id) return &tag;
        } else if (const auto* shape = std::get_if<AfpAnimation::Shape>(&tag.body)) {
            if (shape->id == id) return &tag;
        } else if (const auto* image = std::get_if<AfpAnimation::Image>(&tag.body)) {
            if (image->id == id) return &tag;
        }
    }
    return nullptr;
}

bool Imported(const AfpAnimation::Animation& animation, uint16_t id) {
    return std::ranges::any_of(animation.imports, [id](const AfpAnimation::Import& imported) {
        return std::ranges::any_of(imported.assets, [id](const AfpAnimation::ImportedAsset& asset) {
            return asset.tag == id;
        });
    });
}

std::vector<uint16_t> Referenced(const AfpAnimation::Placement& placement) {
    std::vector<uint16_t> ids;
    if (placement.character) ids.push_back(*placement.character);
    if (placement.grid_controller) ids.push_back(placement.grid_controller->tag);
    return ids;
}

void RenumberPlacement(AfpAnimation::Placement& placement,
                       const std::map<uint16_t, uint16_t>& moved) {
    const auto renumbered = [&moved](uint16_t id) {
        const auto found = moved.find(id);
        return found != moved.end() ? found->second : id;
    };
    if (placement.character) placement.character = renumbered(*placement.character);
    if (placement.grid_controller)
        placement.grid_controller->tag = renumbered(placement.grid_controller->tag);
}

class Closure {
public:
    explicit Closure(const AfpAnimation::Animation& source) : source_(&source) {}

    Support::Expected<void, std::string> Add(uint16_t id) {
        if (std::ranges::find(order_, id) != order_.end()) return {};
        if (std::ranges::find(visiting_, id) != visiting_.end()) {
            return Support::Unexpected("character " + std::to_string(id) +
                                       " places itself through its own sprites");
        }
        if (Imported(*source_, id)) {
            return Support::Unexpected(
                "character " + std::to_string(id) +
                " is imported from another movie, and importing it into this animation is not "
                "something the editor does");
        }
        const AfpAnimation::Tag* tag = Definition(*source_, id);
        if (tag == nullptr) {
            return Support::Unexpected("character " + std::to_string(id) +
                                       " is not defined in the animation it was copied from");
        }
        if (const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag->body)) {
            visiting_.push_back(id);
            auto inside = AddContainer(sprite->container);
            visiting_.pop_back();
            if (!inside) return inside;
        }
        order_.push_back(id);
        return {};
    }

    [[nodiscard]] const std::vector<uint16_t>& Order() const { return order_; }

private:
    Support::Expected<void, std::string> AddContainer(const AfpAnimation::Container& clip) {
        for (const AfpAnimation::Tag& tag : clip.tags) {
            if (std::holds_alternative<AfpAnimation::UnknownTag>(tag.body)) {
                return Support::Unexpected(std::string(
                    "a copied sprite holds a tag the editor does not read, so it cannot say what "
                    "that tag refers to"));
            }
            if (std::holds_alternative<AfpAnimation::Sprite>(tag.body)) {
                return Support::Unexpected(
                    std::string("a copied sprite defines another sprite inside itself"));
            }
            const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
            if (placement == nullptr) continue;
            for (const uint16_t id : Referenced(*placement)) {
                auto added = Add(id);
                if (!added) return added;
            }
        }
        return {};
    }

    const AfpAnimation::Animation* source_;
    std::vector<uint16_t> order_;
    std::vector<uint16_t> visiting_;
};

void Renumber(AfpAnimation::Container& clip, const std::map<uint16_t, uint16_t>& moved) {
    for (AfpAnimation::Tag& tag : clip.tags) {
        auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement != nullptr) RenumberPlacement(*placement, moved);
    }
}

AfpAnimation::Tag Moved(const AfpAnimation::Tag& original, uint16_t id,
                        const std::map<uint16_t, uint16_t>& moved) {
    AfpAnimation::Tag tag = original;
    if (auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body)) {
        sprite->id = id;
        Renumber(sprite->container, moved);
    } else if (auto* shape = std::get_if<AfpAnimation::Shape>(&tag.body)) {
        shape->id = id;
    } else if (auto* image = std::get_if<AfpAnimation::Image>(&tag.body)) {
        image->id = id;
    }
    return tag;
}

Support::Expected<std::map<uint16_t, uint16_t>, std::string>
NewIds(const AfpAnimation::Animation& target, const std::vector<uint16_t>& order) {
    const auto first = NextCharacterId(target);
    if (!first) return Support::Unexpected(first.error());
    if (order.size() > std::numeric_limits<uint16_t>::max() - static_cast<std::size_t>(*first))
        return Support::Unexpected(std::string("the animation has no character ids left"));
    std::map<uint16_t, uint16_t> moved;
    uint16_t next = *first;
    for (const uint16_t id : order)
        moved.emplace(id, next++);
    return moved;
}

void DefineCharacters(AfpAnimation::Animation& target, const AfpAnimation::Animation& source,
                      const std::vector<uint16_t>& order,
                      const std::map<uint16_t, uint16_t>& moved) {
    for (const uint16_t id : order) {
        AfpAnimation::Tag tag = Moved(*Definition(source, id), moved.at(id), moved);
        CarryStrings(tag, source, target);
        InsertTag(target.root, 0, std::move(tag));
    }
}

CopiedSpan Carried(const CopiedSpan& copied, std::string_view animation_path,
                   const std::map<uint16_t, uint16_t>& moved, AfpAnimation::Animation& target) {
    CopiedSpan carried = copied;
    carried.animation = std::string(animation_path);
    for (auto& [offset, placement] : carried.placements) {
        RenumberPlacement(placement, moved);
        AfpAnimation::Tag wrapped{placement};
        CarryStrings(wrapped, copied.source, target);
        placement = std::get<AfpAnimation::Placement>(std::move(wrapped.body));
    }
    return carried;
}

}

Support::Expected<CopiedSpan, std::string> CopySpanFrom(const File& file,
                                                        std::string_view animation_path,
                                                        ClipId clip, uint16_t depth,
                                                        uint32_t frame) {
    auto animation = file.ReadAnimation(animation_path);
    if (!animation) return Support::Unexpected(animation.error());
    auto copied = CopySpan(*animation, animation_path, clip, depth, frame);
    if (!copied) return Support::Unexpected(copied.error());
    for (const AfpAnimation::Tag& tag : animation->root.tags) {
        const auto* shape = std::get_if<AfpAnimation::Shape>(&tag.body);
        if (shape == nullptr) continue;
        std::optional<std::vector<uint8_t>> bytes = file.ShapeFile(animation_path, shape->id);
        if (bytes) copied->shape_files.emplace(shape->id, std::move(*bytes));
    }
    copied->source = std::move(*animation);
    return copied;
}

Support::Expected<Span, std::string> PasteSpanInto(File& file, std::string_view animation_path,
                                                   ClipId clip, const CopiedSpan& copied,
                                                   uint16_t depth, uint32_t first_frame) {
    auto target = file.ReadAnimation(animation_path);
    if (!target) return Support::Unexpected(target.error());
    if (copied.animation == animation_path) {
        auto placed = PasteSpan(*target, animation_path, clip, copied, depth, first_frame);
        if (!placed) return Support::Unexpected(placed.error());
        auto written = file.WriteAnimation(animation_path, *target);
        if (!written) return Support::Unexpected(written.error());
        return *placed;
    }

    Closure closure(copied.source);
    for (const auto& [offset, placement] : copied.placements) {
        for (const uint16_t id : Referenced(placement)) {
            auto added = closure.Add(id);
            if (!added) return Support::Unexpected(added.error());
        }
    }
    auto moved = NewIds(*target, closure.Order());
    if (!moved) return Support::Unexpected(moved.error());

    DefineCharacters(*target, copied.source, closure.Order(), *moved);
    const CopiedSpan carried = Carried(copied, animation_path, *moved, *target);
    auto placed = PasteSpan(*target, animation_path, clip, carried, depth, first_frame);
    if (!placed) return Support::Unexpected(placed.error());

    File edited = file;
    auto written = edited.WriteAnimation(animation_path, *target);
    if (!written) return Support::Unexpected(written.error());
    for (const uint16_t id : closure.Order()) {
        const auto bytes = copied.shape_files.find(id);
        if (bytes == copied.shape_files.end()) continue;
        auto added = edited.AddShapeFile(animation_path, moved->at(id), bytes->second);
        if (!added) return Support::Unexpected(added.error());
    }
    file = std::move(edited);
    return *placed;
}

}
