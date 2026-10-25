#include "document/inputs.h"

#include "document/animation_strings.h"
#include "document/clip.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Document {

namespace {

constexpr int kDeepestWalk = 8;

struct Walk {
    const AfpAnimation::Animation* animation = nullptr;
    const std::map<uint16_t, std::string>* shape_images = nullptr;
    std::map<uint16_t, const AfpAnimation::Container*> sprites;
    InputSurface surface;
    std::map<std::string, std::size_t> found;
};

std::string TextureOf(const Walk& walk, std::optional<uint16_t> character, int depth) {
    if (!character || depth > kDeepestWalk) return {};
    const auto shape = walk.shape_images->find(*character);
    if (shape != walk.shape_images->end()) return shape->second;
    const auto sprite = walk.sprites.find(*character);
    if (sprite == walk.sprites.end()) return {};
    for (const AfpAnimation::Tag& tag : sprite->second->tags) {
        const auto* inside = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (inside == nullptr) continue;
        const std::string found = TextureOf(walk, inside->character, depth + 1);
        if (!found.empty()) return found;
    }
    return {};
}

void CollectSprites(const AfpAnimation::Container& clip, Walk& walk) {
    for (const AfpAnimation::Tag& tag : clip.tags) {
        const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body);
        if (sprite == nullptr) continue;
        walk.sprites.emplace(sprite->id, &sprite->container);
        CollectSprites(sprite->container, walk);
    }
}

uint32_t FramesOf(const Walk& walk, const std::optional<uint16_t>& character) {
    if (!character) return 0;
    const auto found = walk.sprites.find(*character);
    if (found == walk.sprites.end()) return 0;
    return static_cast<uint32_t>(found->second->frames.size());
}

uint32_t FrameOfTag(const AfpAnimation::Container& clip, std::size_t position) {
    for (std::size_t at = 0; at < clip.frames.size(); at++) {
        const AfpAnimation::Frame& frame = clip.frames[at];
        if (position >= frame.first_tag && position < frame.first_tag + frame.tag_count)
            return static_cast<uint32_t>(at);
    }
    return 0;
}

constexpr uint32_t kMostPlaces = 1000000;
constexpr uint32_t kDigits = 10;

std::optional<uint32_t> WeightOf(const std::string& name) {
    const std::size_t cut = name.rfind('_');
    if (cut == std::string::npos || cut + 1 >= name.size()) return std::nullopt;
    const std::string tail = name.substr(cut + 1);
    uint32_t value = 0;
    for (const char one : tail) {
        if (one < '0' || one > '9') return std::nullopt;
        value = (value * kDigits) + static_cast<uint32_t>(one - '0');
        if (value > kMostPlaces) return std::nullopt;
    }
    for (uint32_t place = 1; place <= kMostPlaces; place *= kDigits) {
        if (value == place) return place;
    }
    return std::nullopt;
}

std::optional<std::size_t> DigitPlace(const std::string& texture,
                                      const std::set<std::string>& images) {
    for (std::size_t at = 0; at < texture.size(); at++) {
        if (texture[at] < '0' || texture[at] > '9') continue;
        std::string tried = texture;
        bool whole = true;
        for (uint32_t digit = 0; digit < kDigits && whole; digit++) {
            tried[at] = static_cast<char>('0' + digit);
            whole = images.contains(tried);
        }
        if (whole) return at;
    }
    return std::nullopt;
}

void SortByWeight(InputNumber& number) {
    std::vector<std::size_t> order(number.places.size());
    std::ranges::generate(order, [at = std::size_t{0}]() mutable { return at++; });
    std::ranges::sort(order, [&number](std::size_t left, std::size_t right) {
        return number.weights[left] > number.weights[right];
    });
    std::vector<std::size_t> places;
    std::vector<uint32_t> weights;
    places.reserve(order.size());
    weights.reserve(order.size());
    for (const std::size_t at : order) {
        places.push_back(number.places[at]);
        weights.push_back(number.weights[at]);
    }
    number.places = std::move(places);
    number.weights = std::move(weights);
}

void ReadPlacements(const AfpAnimation::Container& clip, ClipId owner, Walk& walk) {
    for (std::size_t at = 0; at < clip.tags.size(); at++) {
        const auto* placement = std::get_if<AfpAnimation::Placement>(&clip.tags[at].body);
        if (placement == nullptr || !placement->name) continue;
        const std::string name = StringText(*walk.animation, *placement->name);
        if (name.empty()) continue;
        const auto seen = walk.found.find(name);
        if (seen != walk.found.end()) {
            walk.surface.names[seen->second].places++;
            continue;
        }
        const uint32_t frames = FramesOf(walk, placement->character);
        const std::string texture = TextureOf(walk, placement->character, 0);
        walk.found.emplace(name, walk.surface.names.size());
        walk.surface.names.push_back(
            InputSlot{.name = name,
                      .clip = owner,
                      .depth = placement->depth,
                      .frame = FrameOfTag(clip, at),
                      .character = placement->character,
                      .texture = texture,
                      .frames = frames,
                      .places = 1,
                      .driven = frames > 1 ? InputDriven::Frames : InputDriven::Texture});
    }
}

void ReadLabels(const AfpAnimation::Container& clip, ClipId owner, Walk& walk) {
    for (const AfpAnimation::Label& label : clip.labels) {
        const std::string name = StringText(*walk.animation, label.name);
        if (name.empty()) continue;
        walk.surface.labels.push_back(
            InputLabel{.name = name, .clip = owner, .frame = label.frame});
    }
}

void ReadClip(const AfpAnimation::Container& clip, ClipId owner, Walk& walk) {
    ReadPlacements(clip, owner, walk);
    ReadLabels(clip, owner, walk);
    for (const AfpAnimation::Tag& tag : clip.tags) {
        const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body);
        if (sprite == nullptr) continue;
        ReadClip(sprite->container, ClipId{.sprite = sprite->id}, walk);
    }
}

}

InputSurface Inputs(const AfpAnimation::Animation& animation,
                    const std::map<uint16_t, std::string>& shape_images) {
    Walk walk;
    walk.animation = &animation;
    walk.shape_images = &shape_images;
    CollectSprites(animation.root, walk);
    ReadClip(animation.root, ClipId{}, walk);
    return std::move(walk.surface);
}

std::vector<InputNumber> Numbers(const InputSurface& surface,
                                 const std::vector<std::string>& images) {
    const std::set<std::string> known(images.begin(), images.end());
    std::vector<InputNumber> made;
    std::map<std::string, std::size_t> stems;
    for (std::size_t at = 0; at < surface.names.size(); at++) {
        const InputSlot& slot = surface.names[at];
        const std::optional<std::size_t> digit_at = DigitPlace(slot.texture, known);
        if (!digit_at) continue;
        const std::optional<uint32_t> weight = WeightOf(slot.name);
        const std::string stem = weight ? slot.name.substr(0, slot.name.rfind('_')) : slot.name;
        const auto found = stems.find(stem);
        if (found == stems.end()) {
            stems.emplace(stem, made.size());
            made.push_back(InputNumber{.stem = stem,
                                       .digit_at = *digit_at,
                                       .places = {at},
                                       .weights = {weight.value_or(1)}});
            continue;
        }
        InputNumber& growing = made[found->second];
        if (growing.digit_at != *digit_at) continue;
        growing.places.push_back(at);
        growing.weights.push_back(weight.value_or(1));
    }
    for (InputNumber& one : made)
        SortByWeight(one);
    return made;
}

std::string Digit(const std::string& texture, std::size_t digit_at, uint32_t digit) {
    if (digit_at >= texture.size() || digit >= kDigits) return {};
    std::string made = texture;
    made[digit_at] = static_cast<char>('0' + digit);
    return made;
}

uint32_t DigitOf(const std::string& texture, std::size_t digit_at) {
    if (digit_at >= texture.size()) return 0;
    const char one = texture[digit_at];
    return one >= '0' && one <= '9' ? static_cast<uint32_t>(one - '0') : 0;
}

}
