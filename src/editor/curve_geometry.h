#pragma once

#include "preset/doc/preset_document.h"
#include "preset/eval/eval_tween.h"

#include <array>
#include <string>
#include <vector>

namespace Editor {

struct CurveRect {
    float x0 = 0.0F;
    float y0 = 0.0F;
    float x1 = 1.0F;
    float y1 = 1.0F;
};

struct CurveRange {
    double low = 0.0;
    double high = 1.0;
};

struct CurvePoint {
    float x = 0.0F;
    float y = 0.0F;
};

struct CurveSegment {
    int a_at = 0;
    int b_at = 1;
    double a_value = 0.0;
    double b_value = 1.0;
};

struct CurveChannel {
    std::string field;
    int component = -1;
    std::string label;
};

struct CurveSample {
    int frame = 0;
    double value = 0.0;
};

std::vector<CurveChannel> ChannelsOf(const Preset::Doc::Clip& clip);

double ChannelValue(const Preset::Doc::ParamValue& value, int component);

double CurveSampleAt(const Preset::Doc::Clip& clip, const CurveChannel& channel,
                     const Preset::Eval::TweenValue& underlying, int clip_frame);

std::vector<CurveSample> CurveSamples(const Preset::Doc::Clip& clip, const CurveChannel& channel,
                                      const Preset::Eval::TweenValue& underlying, int duration);

CurveRange CurveAutoRange(const Preset::Doc::Clip& clip, const CurveChannel& channel,
                          const Preset::Eval::TweenValue& underlying, int duration);

float CurveX(const CurveRect& rect, int duration, double frame);

double CurveFrame(const CurveRect& rect, int duration, float x);

float CurveY(const CurveRect& rect, const CurveRange& range, double value);

double CurveValue(const CurveRect& rect, const CurveRange& range, float y);

CurveRange RangeOfValues(const std::vector<double>& values);

CurvePoint BezierHandle(const CurveRect& rect, const CurveRange& range, int duration,
                        const CurveSegment& segment, const std::array<double, 4>& cp, int handle);

std::array<double, 4> BezierWithHandle(const CurveRect& rect, const CurveRange& range, int duration,
                                       const CurveSegment& segment, const std::array<double, 4>& cp,
                                       int handle, CurvePoint at);

}
