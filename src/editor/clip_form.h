#pragma once

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_fields.h"

#include <string>
#include <vector>

namespace Editor {

struct FormRow {
    const Preset::Doc::FieldDesc* field = nullptr;
    bool at_default = true;
    std::string default_text = {};
};

std::vector<FormRow> FormRows(const Preset::Doc::Command& command);

bool AtCatalogDefault(const Preset::Doc::Command& command, const Preset::Doc::FieldDesc& field);

Preset::Doc::ParamValue ClampField(const Preset::Doc::FieldDesc& field,
                                   Preset::Doc::ParamValue value);

bool OutsideSoftRange(const Preset::Doc::FieldDesc& field, const Preset::Doc::ParamValue& value);

std::string DegreesText(const Preset::Doc::FieldDesc& field, const Preset::Doc::ParamValue& value);

}
