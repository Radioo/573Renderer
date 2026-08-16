#pragma once

#include "preset/doc/preset_document.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Editor {

enum class PaletteSection : uint8_t {
    Track,
    OtherTracks,
    Document,
};

struct PaletteEntry {
    Preset::Doc::CommandType type = Preset::Doc::CommandType::SpriteDraw;
    PaletteSection section = PaletteSection::Track;
    std::string name = {};
    std::string summary = {};
    std::string refusal = {};
    bool enabled = true;
};

std::vector<PaletteEntry> PaletteEntries(const Preset::Doc::Document& document,
                                         std::string_view track_id, int frame,
                                         std::string_view filter);

std::string InsertPaletteCommand(Preset::Doc::Document& document, std::string_view track_id,
                                 Preset::Doc::CommandType type, int frame);

struct TrackSpec {
    Preset::Doc::TrackKind kind = Preset::Doc::TrackKind::Model;
    std::string target = {};
    std::string name = {};
    bool below_selected = true;
};

std::string InsertTrack(Preset::Doc::Document& document, const TrackSpec& spec,
                        std::string_view selected_track_id);

}
