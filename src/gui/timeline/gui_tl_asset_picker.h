#pragma once

#include "gui_tl_forms.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"

#include <string>

namespace Panels::Timeline {

std::string CommandAsset(const Preset::Doc::Command& command);

FormContext MakeFormContext(const Preset::Doc::Document& document,
                            const Preset::Doc::Command& command, const std::string& target);

FieldEvent DrawAssetTab(const FormContext& context, Preset::Doc::Document& document,
                        Preset::Doc::Command& command);

FieldEvent DrawHiddenParts(const FormContext& context, Preset::Doc::Command& command);

bool HasAssetField(Preset::Doc::CommandType type);

}
