#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

namespace Preset {

enum class ValueKind : uint8_t { Bool, Int, Enum, Float, Vec3, Color };

struct Value {
    ValueKind kind = ValueKind::Float;
    std::array<float, 3> f = {0.0F, 0.0F, 0.0F};
    int i = 0;
};

struct Range {
    float min = 0.0F;
    float max = 0.0F;
    float step = 0.0F;
    bool soft = false;
};

enum class Scope : uint8_t {
    Scene,
    Camera,
    Countdown,
    Intro,
    Beat,
    Pulse,
    Jitter,
    Light,
    Model,
    ModelMotion,
    Sprite,
    SpriteTiming,
    Option,
    OptionChoice,
};

constexpr int16_t kNoIndex = -1;

struct Target {
    Scope scope = Scope::Scene;
    int16_t index = kNoIndex;
    int16_t sub = -1;
};

struct Accessor {
    Value (*get)(const void* owner) = nullptr;
    void (*set)(void* owner, const Value& value) = nullptr;
};

enum class Apply : uint8_t { Live, Rebind };

struct ParamDesc {
    std::string_view key = {};
    std::string_view label = {};
    std::string_view group = {};
    ValueKind kind = ValueKind::Float;
    Range range = {};
    Apply apply = Apply::Live;
    std::string_view unit = {};
    std::string_view help = {};
    std::string_view aliases = {};
    std::span<const std::string_view> enum_labels = {};
    Scope scope = Scope::Scene;
    Accessor accessor = {};
};

Value ToValue(float v);
Value ToValue(int v);
Value ToValue(bool v);
Value ToValue(const std::array<float, 3>& v);

void FromValue(const Value& value, float& out);
void FromValue(const Value& value, int& out);
void FromValue(const Value& value, bool& out);
void FromValue(const Value& value, std::array<float, 3>& out);

template <class E>
    requires std::is_enum_v<E>
Value ToValue(E v) {
    return ToValue((int)v);
}

template <class E>
    requires std::is_enum_v<E>
void FromValue(const Value& value, E& out) {
    int raw = (int)out;
    FromValue(value, raw);
    out = (E)raw;
}

template <class T> struct MemberOf;

template <class O, class F> struct MemberOf<F O::*> {
    using Owner = O;
    using Type = F;
};

template <Scope S> struct ScopeOwner;

Value ClampValue(const ParamDesc& desc, const Value& value);

bool SameValue(const Value& a, const Value& b);

std::span<const ParamDesc> Schema();

template <Scope S, auto Member> constexpr ParamDesc Row(ParamDesc desc) {
    using Owner = typename MemberOf<decltype(Member)>::Owner;
    static_assert(std::is_same_v<Owner, typename ScopeOwner<S>::type>,
                  "parameter row scope does not match the member's owner type");
    desc.scope = S;
    desc.accessor = Accessor{
        .get = [](const void* owner) { return ToValue(static_cast<const Owner*>(owner)->*Member); },
        .set = [](void* owner,
                  const Value& value) { FromValue(value, static_cast<Owner*>(owner)->*Member); }};
    return desc;
}

}
