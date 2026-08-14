#include "preset/preset_params.h"

#include <array>
#include <cstddef>
#include <algorithm>
#include <cmath>

namespace Preset {

namespace {

float ClampScalar(const Range& range, float v) {
    if (range.min == 0.0F && range.max == 0.0F) return v;
    if (range.soft) return v;
    return std::clamp(v, range.min, range.max);
}

}

Value ToValue(float v) {
    return Value{.kind = ValueKind::Float, .f = {v, 0.0F, 0.0F}};
}

Value ToValue(int v) {
    return Value{.kind = ValueKind::Int, .i = v};
}

Value ToValue(bool v) {
    return Value{.kind = ValueKind::Bool, .i = v ? 1 : 0};
}

Value ToValue(const std::array<float, 3>& v) {
    return Value{.kind = ValueKind::Vec3, .f = v};
}

void FromValue(const Value& value, float& out) {
    out = value.f[0];
}

void FromValue(const Value& value, int& out) {
    out = value.i;
}

void FromValue(const Value& value, bool& out) {
    out = value.i != 0;
}

void FromValue(const Value& value, std::array<float, 3>& out) {
    out = value.f;
}

Value ClampValue(const ParamDesc& desc, const Value& value) {
    Value out = value;
    out.kind = desc.kind;
    switch (desc.kind) {
    case ValueKind::Bool:
        out.i = (value.i != 0) ? 1 : 0;
        break;
    case ValueKind::Enum:
        out.i = std::clamp(value.i, 0, std::max(0, (int)desc.enum_labels.size() - 1));
        break;
    case ValueKind::Int:
        out.i = (int)std::lround(ClampScalar(desc.range, (float)value.i));
        break;
    case ValueKind::Float:
        out.f[0] = ClampScalar(desc.range, value.f[0]);
        break;
    case ValueKind::Vec3:
    case ValueKind::Color:
        for (float& component : out.f)
            component = ClampScalar(desc.range, component);
        break;
    }
    return out;
}

bool SameValue(const Value& a, const Value& b) {
    if (a.kind != b.kind) return false;
    switch (a.kind) {
    case ValueKind::Bool:
    case ValueKind::Int:
    case ValueKind::Enum:
        return a.i == b.i;
    case ValueKind::Float:
        return std::abs(a.f[0] - b.f[0]) <= 1e-6F;
    case ValueKind::Vec3:
    case ValueKind::Color:
        for (size_t i = 0; i < a.f.size(); i++) {
            if (std::abs(a.f[i] - b.f[i]) > 1e-6F) return false;
        }
        return true;
    }
    return false;
}

}
