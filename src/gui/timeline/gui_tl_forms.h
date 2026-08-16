#pragma once

#include "preset/asset_index.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_fields.h"

#include <memory>
#include <string>
#include <vector>

namespace Panels::Timeline {

struct FormContext {
    std::shared_ptr<const Preset::AssetIndex> assets;
    std::string asset_id = {};
    std::string target = {};
    std::vector<std::string> asset_ids = {};
};

enum class FieldEvent : unsigned char {
    None,
    Changed,
    Committed,
};

FieldEvent DrawField(const FormContext& context, const Preset::Doc::FieldDesc& field,
                     Preset::Doc::Command& command);

FieldEvent DrawParamForm(const FormContext& context, Preset::Doc::Command& command);

FieldEvent DrawTweenTab(Preset::Doc::Document& document, const std::string& clip_id, int playhead);

FieldEvent DrawVec3Row(const char* id, const char* label, Preset::Doc::Vec3& value, float speed,
                       const char* unit);

FieldEvent DrawDoubleRow(const char* id, const char* label, double& value, float speed,
                         const char* unit);

FieldEvent DrawIntRow(const char* id, const char* label, int& value);

FieldEvent DrawEnumRow(const char* id, const char* label, int& value,
                       const std::vector<std::string>& names);

void RowLabel(const char* label);

const Preset::AssetEntry* FindAsset(const FormContext& context, const std::string& asset_id);

}
