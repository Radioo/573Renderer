#pragma once

#include "preset/doc/preset_document.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Editor {

inline constexpr int kTransitionFrames = 30;

const Preset::Doc::Clip* ClipById(const Preset::Doc::Document& document, std::string_view clip_id);

int ClipDuration(const Preset::Doc::Document& document, std::string_view clip_id);

const Preset::Doc::KeyValue* KeyValueOf(const Preset::Doc::Key& key, std::string_view field_id);

struct TweenField {
    std::string id = {};
    std::string target_id = {};
};

std::vector<TweenField> TweenFields(const Preset::Doc::Document& document,
                                    std::string_view clip_id);

std::optional<Preset::Doc::ParamValue> ResolvedFieldValue(const Preset::Doc::Document& document,
                                                          std::string_view clip_id,
                                                          std::string_view field_id, int frame);

int AddKeyAt(Preset::Doc::Document& document, std::string_view clip_id, int at);

bool MoveKey(Preset::Doc::Document& document, std::string_view clip_id, int index, int at);

bool DeleteKey(Preset::Doc::Document& document, std::string_view clip_id, int index);

bool SetKeyEase(Preset::Doc::Document& document, std::string_view clip_id, int index,
                Preset::Doc::Ease ease);

bool SetKeyRate(Preset::Doc::Document& document, std::string_view clip_id, int index,
                double rate_deg);

bool SetKeyBezier(Preset::Doc::Document& document, std::string_view clip_id, int index,
                  const std::array<double, 4>& cp);

bool SetKeyValue(Preset::Doc::Document& document, std::string_view clip_id, int index,
                 std::string field_id, Preset::Doc::ParamValue value);

bool UnsetKeyValue(Preset::Doc::Document& document, std::string_view clip_id, int index,
                   std::string_view field_id);

std::string NextDrawClip(const Preset::Doc::Document& document, std::string_view clip_id);

struct TransitionInsert {
    std::string tween_id = {};
    std::string bridge_id = {};
    [[nodiscard]] bool Valid() const { return !tween_id.empty(); }
};

TransitionInsert AddTransition(Preset::Doc::Document& document, std::string_view a_id,
                               std::string_view b_id, int frames);

}
