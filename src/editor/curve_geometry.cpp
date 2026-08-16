#include "editor/curve_geometry.h"

#include "editor/tween_edits.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/eval/eval_tween.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <variant>
#include <vector>

namespace Editor {

namespace Doc = Preset::Doc;

namespace {

constexpr double kMinPad = 0.05;
constexpr double kPadFraction = 0.1;
constexpr int kMaxSamples = 240;

double Span(int duration) {
    return (double)std::max(1, duration);
}

}

std::vector<CurveChannel> ChannelsOf(const Doc::Clip& clip) {
    std::vector<CurveChannel> out;
    for (const Doc::Key& key : clip.keys) {
        for (const Doc::KeyValue& value : key.values) {
            const bool known = std::ranges::any_of(
                out, [&value](const CurveChannel& channel) { return channel.field == value.id; });
            if (known) continue;
            if (std::holds_alternative<Doc::Vec3>(value.value)) {
                for (int axis = 0; axis < 3; axis++) {
                    const std::string suffix = std::string(".") + "xyz"[axis];
                    out.push_back(CurveChannel{
                        .field = value.id, .component = axis, .label = value.id + suffix});
                }
                continue;
            }
            out.push_back(CurveChannel{.field = value.id, .component = -1, .label = value.id});
        }
    }
    return out;
}

double ChannelValue(const Doc::ParamValue& value, int component) {
    if (const auto* scalar = std::get_if<double>(&value)) return *scalar;
    if (const auto* integer = std::get_if<int>(&value)) return (double)*integer;
    const auto* vector = std::get_if<Doc::Vec3>(&value);
    if (vector == nullptr) return 0.0;
    return (*vector)[(std::size_t)std::clamp(component, 0, 2)];
}

double CurveSampleAt(const Doc::Clip& clip, const CurveChannel& channel,
                     const Preset::Eval::TweenValue& underlying, int clip_frame) {
    Preset::Eval::TweenValue out = underlying;
    if (!Preset::Eval::SampleKeys(clip.keys, channel.field, clip_frame, underlying, out))
        return 0.0;
    switch (out.kind) {
    case Preset::Eval::TweenValue::Kind::Vector:
        return (double)out.vector[(std::size_t)std::clamp(channel.component, 0, 2)];
    case Preset::Eval::TweenValue::Kind::Integer:
        return (double)out.integer;
    case Preset::Eval::TweenValue::Kind::Scalar:
    default:
        return (double)out.scalar;
    }
}

std::vector<CurveSample> CurveSamples(const Doc::Clip& clip, const CurveChannel& channel,
                                      const Preset::Eval::TweenValue& underlying, int duration) {
    const int span = std::max(1, duration);
    const int step = std::max(1, span / kMaxSamples);
    std::vector<CurveSample> out;
    for (int frame = 0; frame <= span; frame += step) {
        out.push_back(
            CurveSample{.frame = frame, .value = CurveSampleAt(clip, channel, underlying, frame)});
    }
    if (out.back().frame == span) return out;
    out.push_back(
        CurveSample{.frame = span, .value = CurveSampleAt(clip, channel, underlying, span)});
    return out;
}

CurveRange CurveAutoRange(const Doc::Clip& clip, const CurveChannel& channel,
                          const Preset::Eval::TweenValue& underlying, int duration) {
    std::vector<double> values;
    for (const Doc::Key& key : clip.keys) {
        const Doc::KeyValue* held = KeyValueOf(key, channel.field);
        if (held != nullptr) values.push_back(ChannelValue(held->value, channel.component));
    }
    for (const CurveSample& sample : CurveSamples(clip, channel, underlying, duration))
        values.push_back(sample.value);
    return RangeOfValues(values);
}

float CurveX(const CurveRect& rect, int duration, double frame) {
    const double fraction = frame / Span(duration);
    return rect.x0 + (float)(fraction * (double)(rect.x1 - rect.x0));
}

double CurveFrame(const CurveRect& rect, int duration, float x) {
    const float width = rect.x1 - rect.x0;
    if (width <= 0.0F) return 0.0;
    return (double)((x - rect.x0) / width) * Span(duration);
}

float CurveY(const CurveRect& rect, const CurveRange& range, double value) {
    const double span = range.high - range.low;
    if (span <= 0.0) return rect.y1;
    const double fraction = (value - range.low) / span;
    return rect.y1 - (float)(fraction * (double)(rect.y1 - rect.y0));
}

double CurveValue(const CurveRect& rect, const CurveRange& range, float y) {
    const float height = rect.y1 - rect.y0;
    if (height <= 0.0F) return range.low;
    return range.low + ((double)((rect.y1 - y) / height) * (range.high - range.low));
}

CurveRange RangeOfValues(const std::vector<double>& values) {
    if (values.empty()) return {};
    double low = values.front();
    double high = values.front();
    for (const double value : values) {
        low = std::min(low, value);
        high = std::max(high, value);
    }
    const double pad = std::max(kMinPad, (high - low) * kPadFraction);
    return CurveRange{.low = low - pad, .high = high + pad};
}

CurvePoint BezierHandle(const CurveRect& rect, const CurveRange& range, int duration,
                        const CurveSegment& segment, const std::array<double, 4>& cp, int handle) {
    const auto axis = (std::size_t)((handle == 0) ? 0 : 2);
    const double frames = std::max(1, segment.b_at - segment.a_at);
    const double frame = (double)segment.a_at + (cp[axis] * frames);
    const double value = segment.a_value + (cp[axis + 1] * (segment.b_value - segment.a_value));
    return CurvePoint{.x = CurveX(rect, duration, frame), .y = CurveY(rect, range, value)};
}

std::array<double, 4> BezierWithHandle(const CurveRect& rect, const CurveRange& range, int duration,
                                       const CurveSegment& segment, const std::array<double, 4>& cp,
                                       int handle, CurvePoint at) {
    std::array<double, 4> out = cp;
    const auto axis = (std::size_t)((handle == 0) ? 0 : 2);
    const double frames = std::max(1, segment.b_at - segment.a_at);
    const double frame = CurveFrame(rect, duration, at.x);
    out[axis] = std::clamp((frame - (double)segment.a_at) / frames, 0.0, 1.0);

    const double reach = segment.b_value - segment.a_value;
    if (reach == 0.0) return out;
    out[axis + 1] = (CurveValue(rect, range, at.y) - segment.a_value) / reach;
    return out;
}

}
