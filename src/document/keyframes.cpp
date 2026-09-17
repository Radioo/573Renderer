#include "document/keyframes.h"

#include "support/expected.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Document {

namespace {

constexpr int kSolveSteps = 40;

constexpr std::array<std::string_view, 3> kEaseNames{"hold", "linear", "bezier"};

double CurveAt(double a, double b, double u) {
    const double rest = 1.0 - u;
    return (3.0 * rest * rest * u * a) + (3.0 * rest * u * u * b) + (u * u * u);
}

double BezierFraction(const Bezier& bezier, double t) {
    double low = 0.0;
    double high = 1.0;
    for (int step = 0; step < kSolveSteps; step++) {
        const double middle = 0.5 * (low + high);
        if (CurveAt(bezier.x1, bezier.x2, middle) < t) {
            low = middle;
        } else {
            high = middle;
        }
    }
    return CurveAt(bezier.y1, bezier.y2, 0.5 * (low + high));
}

double Fraction(const Keyframe& from, double t) {
    switch (from.ease) {
    case Ease::Hold:
        return 0.0;
    case Ease::Linear:
        return t;
    case Ease::Bezier:
        return BezierFraction(from.bezier, t);
    }
    return t;
}

int64_t Blend(int64_t from, int64_t to, double fraction) {
    const double value = static_cast<double>(from) +
                         ((static_cast<double>(to) - static_cast<double>(from)) * fraction);
    return std::llround(value);
}

std::vector<Keyframe>::iterator Find(Track& track, uint32_t frame) {
    return std::ranges::find(track.keys, frame, &Keyframe::frame);
}

Support::Expected<void, std::string> CheckBezier(const Bezier& bezier) {
    if (bezier.x1 < 0.0 || bezier.x1 > 1.0 || bezier.x2 < 0.0 || bezier.x2 > 1.0) {
        return Support::Unexpected(
            std::string("an ease control point's time has to be between 0 and 1"));
    }
    if (!std::isfinite(bezier.y1) || !std::isfinite(bezier.y2))
        return Support::Unexpected(std::string("an ease control point has to be a number"));
    return {};
}

}

std::string_view EaseName(Ease ease) {
    return kEaseNames[static_cast<std::size_t>(ease)];
}

std::optional<Ease> EaseFor(std::string_view name) {
    const auto found = std::ranges::find(kEaseNames, name);
    if (found == kEaseNames.end()) return std::nullopt;
    return static_cast<Ease>(std::distance(kEaseNames.begin(), found));
}

Support::Expected<void, std::string> CheckTrack(const Track& track) {
    if (track.property.empty())
        return Support::Unexpected(std::string("a track names no property"));
    if (track.keys.empty())
        return Support::Unexpected("the track for " + track.property + " has no keyframes");
    const std::size_t arity = track.keys.front().value.size();
    if (arity == 0) return Support::Unexpected("the track for " + track.property + " keys nothing");
    for (std::size_t i = 0; i < track.keys.size(); i++) {
        const Keyframe& key = track.keys[i];
        if (key.value.size() != arity) {
            return Support::Unexpected("the keyframes of " + track.property +
                                       " do not all hold the same number of values");
        }
        if (i > 0 && key.frame <= track.keys[i - 1].frame) {
            return Support::Unexpected("the keyframes of " + track.property +
                                       " are not in frame order");
        }
        auto shaped = CheckBezier(key.bezier);
        if (!shaped) return Support::Unexpected(shaped.error());
    }
    return {};
}

std::vector<int64_t> SampleTrack(const Track& track, uint32_t frame) {
    if (track.keys.empty()) return {};
    if (frame <= track.keys.front().frame) return track.keys.front().value;
    if (frame >= track.keys.back().frame) return track.keys.back().value;

    std::size_t after = 0;
    while (after < track.keys.size() && track.keys[after].frame <= frame)
        after++;
    const Keyframe& from = track.keys[after - 1];
    const Keyframe& to = track.keys[after];
    const double span = static_cast<double>(to.frame) - static_cast<double>(from.frame);
    const double t = (static_cast<double>(frame) - static_cast<double>(from.frame)) / span;
    const double fraction = Fraction(from, t);

    std::vector<int64_t> out;
    out.reserve(from.value.size());
    for (std::size_t i = 0; i < from.value.size(); i++) {
        const int64_t target = i < to.value.size() ? to.value[i] : from.value[i];
        out.push_back(Blend(from.value[i], target, fraction));
    }
    return out;
}

Support::Expected<void, std::string> AddKeyframe(Track& track, const Keyframe& key) {
    if (key.value.empty())
        return Support::Unexpected(std::string("a keyframe holds at least one value"));
    if (!track.keys.empty() && key.value.size() != track.keys.front().value.size()) {
        return Support::Unexpected("the track for " + track.property + " keys " +
                                   std::to_string(track.keys.front().value.size()) +
                                   " values, not " + std::to_string(key.value.size()));
    }
    auto shaped = CheckBezier(key.bezier);
    if (!shaped) return Support::Unexpected(shaped.error());
    if (Find(track, key.frame) != track.keys.end()) {
        return Support::Unexpected("frame " + std::to_string(key.frame) +
                                   " already holds a keyframe");
    }
    const auto at = std::ranges::upper_bound(track.keys, key.frame, {}, &Keyframe::frame);
    track.keys.insert(at, key);
    return {};
}

Support::Expected<void, std::string> SetKeyframeValue(Track& track, uint32_t frame,
                                                      const std::vector<int64_t>& value) {
    const auto found = Find(track, frame);
    if (found == track.keys.end())
        return Support::Unexpected("frame " + std::to_string(frame) + " holds no keyframe");
    if (value.size() != found->value.size()) {
        return Support::Unexpected("the track for " + track.property + " keys " +
                                   std::to_string(found->value.size()) + " values, not " +
                                   std::to_string(value.size()));
    }
    found->value = value;
    return {};
}

Support::Expected<void, std::string> SetKeyframeEase(Track& track, uint32_t frame, Ease ease,
                                                     Bezier bezier) {
    const auto found = Find(track, frame);
    if (found == track.keys.end())
        return Support::Unexpected("frame " + std::to_string(frame) + " holds no keyframe");
    auto shaped = CheckBezier(bezier);
    if (!shaped) return Support::Unexpected(shaped.error());
    found->ease = ease;
    found->bezier = bezier;
    return {};
}

Support::Expected<void, std::string> RemoveKeyframe(Track& track, uint32_t frame) {
    const auto found = Find(track, frame);
    if (found == track.keys.end())
        return Support::Unexpected("frame " + std::to_string(frame) + " holds no keyframe");
    if (track.keys.size() == 1) {
        return Support::Unexpected("the track for " + track.property +
                                   " keeps at least one keyframe");
    }
    track.keys.erase(found);
    return {};
}

}
