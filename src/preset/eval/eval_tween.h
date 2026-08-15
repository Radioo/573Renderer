#pragma once

#include "preset/doc/preset_document.h"
#include "preset/eval/frame_state.h"

#include <string_view>
#include <vector>

namespace Preset::Eval {

struct TweenValue {
    enum class Kind : unsigned char {
        Scalar,
        Vector,
        Integer,
    };

    Kind kind = Kind::Scalar;
    float scalar = 0.0F;
    Vec3f vector = {0.0F, 0.0F, 0.0F};
    int integer = 0;
};

float EaseFactor(const Doc::Key& key, int frame, int from_at, int to_at);

TweenValue BlendValues(const TweenValue& from, const TweenValue& to, float factor);

bool SampleKeys(const std::vector<Doc::Key>& keys, std::string_view id, int clip_frame,
                const TweenValue& underlying, TweenValue& out);

TweenValue FromParamValue(const Doc::ParamValue& value);

}
