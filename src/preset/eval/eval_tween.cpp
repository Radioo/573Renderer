#include "preset/eval/eval_tween.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/eval/frame_state.h"
#include "support/math/float_trig.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <variant>
#include <vector>

namespace Preset::Eval {

namespace {

constexpr float kDegrees = 0.017453292F;
constexpr int kNewtonSteps = 8;
constexpr int kBisectSteps = 8;

float BezierAxis(float p1, float p2, float s) {
    const float inv = 1.0F - s;
    return (3.0F * inv * inv * s * p1) + (3.0F * inv * s * s * p2) + (s * s * s);
}

float BezierSlope(float p1, float p2, float s) {
    const float inv = 1.0F - s;
    return (3.0F * inv * inv * p1) + (6.0F * inv * s * (p2 - p1)) + (3.0F * s * s * (1.0F - p2));
}

float BezierFactor(const std::array<double, 4>& cp, float t) {
    const auto x1 = (float)cp[0];
    const auto y1 = (float)cp[1];
    const auto x2 = (float)cp[2];
    const auto y2 = (float)cp[3];
    float s = t;
    for (int i = 0; i < kNewtonSteps; i++) {
        const float slope = BezierSlope(x1, x2, s);
        if (slope == 0.0F) break;
        s -= (BezierAxis(x1, x2, s) - t) / slope;
    }
    if (s < 0.0F || s > 1.0F) {
        float low = 0.0F;
        float high = 1.0F;
        s = t;
        for (int i = 0; i < kBisectSteps; i++) {
            s = (low + high) * 0.5F;
            if (BezierAxis(x1, x2, s) < t) {
                low = s;
            } else {
                high = s;
            }
        }
    }
    return BezierAxis(y1, y2, s);
}

float NormalisedTime(int frame, int from_at, int span) {
    const int elapsed = std::clamp(frame - from_at, 0, span);
    return (float)elapsed / (float)span;
}

const Doc::KeyValue* FindKeyValue(const Doc::Key& key, std::string_view id) {
    for (const Doc::KeyValue& value : key.values) {
        if (value.id == id) return &value;
    }
    return nullptr;
}

}

float EaseFactor(const Doc::Key& key, int frame, int from_at, int to_at) {
    const int span = std::max(1, to_at - from_at);
    switch (key.ease) {
    case Doc::Ease::Hold:
        return (frame >= to_at) ? 1.0F : 0.0F;
    case Doc::Ease::Linear:
        return NormalisedTime(frame, from_at, span);
    case Doc::Ease::SineDeg: {
        const int elapsed = std::clamp(frame - from_at, 0, span);
        const auto degrees = (float)elapsed * (float)key.rate_deg.value_or(0.0);
        return Support::Sinf(degrees * kDegrees);
    }
    case Doc::Ease::EaseIn: {
        const float t = NormalisedTime(frame, from_at, span);
        return t * t;
    }
    case Doc::Ease::EaseOut: {
        const float t = NormalisedTime(frame, from_at, span);
        return 1.0F - ((1.0F - t) * (1.0F - t));
    }
    case Doc::Ease::EaseInOut: {
        const float t = NormalisedTime(frame, from_at, span);
        if (t < 0.5F) return 2.0F * t * t;
        return 1.0F - (2.0F * (1.0F - t) * (1.0F - t));
    }
    case Doc::Ease::Bezier: {
        const float t = NormalisedTime(frame, from_at, span);
        if (!key.cp.has_value()) return t;
        return BezierFactor(*key.cp, t);
    }
    }
    return NormalisedTime(frame, from_at, span);
}

TweenValue BlendValues(const TweenValue& from, const TweenValue& to, float factor) {
    TweenValue out = to;
    if (to.kind == TweenValue::Kind::Integer) return out;
    if (to.kind == TweenValue::Kind::Vector) {
        for (std::size_t i = 0; i < out.vector.size(); i++)
            out.vector[i] = from.vector[i] + ((to.vector[i] - from.vector[i]) * factor);
        return out;
    }
    out.scalar = from.scalar + ((to.scalar - from.scalar) * factor);
    return out;
}

TweenValue FromParamValue(const Doc::ParamValue& value) {
    TweenValue out;
    if (const auto* held = std::get_if<double>(&value)) {
        out.kind = TweenValue::Kind::Scalar;
        out.scalar = (float)*held;
        return out;
    }
    if (const auto* held = std::get_if<Doc::Vec3>(&value)) {
        out.kind = TweenValue::Kind::Vector;
        for (std::size_t i = 0; i < out.vector.size(); i++)
            out.vector[i] = (float)(*held)[i];
        return out;
    }
    if (const auto* held = std::get_if<int>(&value)) {
        out.kind = TweenValue::Kind::Integer;
        out.integer = *held;
        return out;
    }
    return out;
}

bool SampleKeys(const std::vector<Doc::Key>& keys, std::string_view id, int clip_frame,
                const TweenValue& underlying, TweenValue& out) {
    std::vector<std::size_t> named;
    for (std::size_t i = 0; i < keys.size(); i++) {
        if (FindKeyValue(keys[i], id) != nullptr) named.push_back(i);
    }
    if (named.empty()) return false;

    const std::size_t first = named.front();
    const TweenValue first_value = FromParamValue(FindKeyValue(keys[first], id)->value);
    if (clip_frame <= keys[first].at) {
        if (first == 0 || keys[first].at == keys.front().at) {
            out = first_value;
            return true;
        }
        const Doc::Key& lead = keys.front();
        const float factor = EaseFactor(lead, clip_frame, lead.at, keys[first].at);
        out = BlendValues(underlying, first_value, factor);
        return true;
    }

    for (std::size_t step = 0; step + 1 < named.size(); step++) {
        const Doc::Key& from = keys[named[step]];
        const Doc::Key& to = keys[named[step + 1]];
        if (clip_frame >= to.at) continue;
        const TweenValue a = FromParamValue(FindKeyValue(from, id)->value);
        const TweenValue b = FromParamValue(FindKeyValue(to, id)->value);
        out = BlendValues(a, b, EaseFactor(from, clip_frame, from.at, to.at));
        return true;
    }

    if (named.size() == 1) {
        out = first_value;
        return true;
    }
    const Doc::Key& from = keys[named[named.size() - 2]];
    const Doc::Key& to = keys[named.back()];
    const TweenValue a = FromParamValue(FindKeyValue(from, id)->value);
    const TweenValue b = FromParamValue(FindKeyValue(to, id)->value);
    out = BlendValues(a, b, EaseFactor(from, to.at, from.at, to.at));
    return true;
}

}
