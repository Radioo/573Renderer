#pragma once

#include "preset/doc/preset_document.h"

#include <string>
#include <string_view>
#include <vector>

namespace Editor {

inline constexpr std::string_view kOptionBandRow = "###tl_options_band_row";

std::string TransitionSummary(const Preset::Doc::Transition& transition);

int TransitionSpanFrames(const Preset::Doc::Transition& transition);

std::vector<std::string> MovedTargets(const Preset::Doc::OptionSpec& option, int from, int to);

std::vector<std::string> TrackIdsForTargets(const Preset::Doc::Document& document,
                                            const std::vector<std::string>& targets);

std::vector<std::string> ChoiceValueTargets(const Preset::Doc::Document& document);

int AddOption(Preset::Doc::Document& document);

bool RenameChoice(Preset::Doc::Document& document, int option, int choice,
                  const std::string& label);

bool MoveChoice(Preset::Doc::Document& document, int option, int choice, int delta);

}
