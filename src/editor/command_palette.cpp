#include "editor/command_palette.h"

#include "editor/timeline_edits.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_fields.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <utility>
#include <vector>

namespace Editor {

namespace Doc = Preset::Doc;

namespace {

constexpr int kDefaultDuration = 60;

struct Blurb {
    std::string_view name;
    std::string_view summary;
};

constexpr std::array<Blurb, Doc::kCommandTypeNames.size()> kBlurbs = {
    Blurb{.name = "Draw 2D cell",
          .summary = "asset and cell, position, alpha, scale, blend, priority"},
    Blurb{.name = "Play 2D animation",
          .summary = "asset and animation, position, playback, speed, hidden parts"},
    Blurb{.name = "Scroll 2D layer",
          .summary = "modifier: wrap scroll speed, wrap width and start offset"},
    Blurb{.name = "Emitter", .summary = "particle cell, spawn rule, ring reach, life and blend"},
    Blurb{.name = "Draw model", .summary = "pose, blend, spin rate and clip time"},
    Blurb{.name = "Transform tween",
          .summary =
              "modifier over the draw clip: keys on position, rotation, scale, alpha, speed, spin"},
    Blurb{.name = "Motion", .summary = "modifier: orbit, spin kick and beat pulse"},
    Blurb{.name = "Camera set", .summary = "eye, at, up, fov, near and far planes, aspect"},
    Blurb{.name = "Camera tween", .summary = "modifier: keys on eye, at, up, fov and the planes"},
    Blurb{.name = "Light", .summary = "slot index, direction, diffuse and specular colour"},
    Blurb{.name = "Parameter override", .summary = "one schema id and the value it forces"},
    Blurb{.name = "Render settings",
          .summary = "shading and the priority the 2D layers split around the 3D"},
    Blurb{.name = "RNG seed",
          .summary = "event: reseed the shared stream and clear the particle pool"},
    Blurb{.name = "Beat grid", .summary = "rate, span and the offsets of the two grids"},
    Blurb{.name = "Jitter", .summary = "modifier: random shake span, scale, models and mode"},
    Blurb{.name = "Select option",
          .summary = "event: change a choice at this frame during playback and export"},
    Blurb{.name = "Fog",
          .summary = "linear vertex fog around the model pass: colour, start, end, density"},
    Blurb{.name = "Clear colour cycle",
          .summary = "modifier: the strobe windows and grey ramp that drive the frame clear"},
    Blurb{.name = "Camera ease",
          .summary = "modifier: eye and at chase a target a fraction of the gap per frame"},
    Blurb{.name = "Model ease",
          .summary = "modifier: scale, position and alpha chase a target per frame"},
    Blurb{.name = "Camera roll",
          .summary = "modifier: the up vector turns about the view axis every frame"},
    Blurb{.name = "Movie tile grid",
          .summary = "a grid of spinning, orbiting quads textured with a movie file"},
};

std::string Lower(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text)
        out += (char)std::tolower((unsigned char)c);
    return out;
}

bool Matches(const Blurb& blurb, const std::string& needle) {
    if (needle.empty()) return true;
    return Lower(blurb.name).find(needle) != std::string::npos ||
           Lower(blurb.summary).find(needle) != std::string::npos;
}

const Doc::Track* TrackById(const Doc::Document& document, std::string_view track_id) {
    const int index = FindTrack(document, track_id);
    if (index < 0) return nullptr;
    return &document.tracks[(std::size_t)index];
}

PaletteSection SectionFor(Doc::CommandType type, const Doc::Track* track) {
    if (type == Doc::CommandType::OptionSelect) return PaletteSection::Document;
    if (track != nullptr && Doc::TraitsFor(type).kind == track->kind) return PaletteSection::Track;
    return PaletteSection::OtherTracks;
}

std::optional<int> DurationEnd(const Doc::Document& document, const Doc::Track* track,
                               Doc::CommandType type, int frame) {
    if (Doc::IsEvent(type)) return std::nullopt;
    int next = -1;
    if (track != nullptr) {
        for (const Doc::Clip& clip : track->clips) {
            if (clip.start <= frame) continue;
            if (next < 0 || clip.start < next) next = clip.start;
        }
    }
    if (next > frame) return next;
    if (Doc::TraitsFor(type).family != Doc::Family::None) return std::nullopt;
    return std::min(frame + kDefaultDuration, std::max(frame + 1, DocumentLength(document)));
}

std::string AssetForKind(const Doc::Document& document, Doc::AssetKind kind) {
    for (const Doc::Asset& asset : document.assets) {
        if (asset.kind == kind) return asset.id;
    }
    return {};
}

void FillAsset(const Doc::Document& document, Doc::Command& command) {
    const std::string scene = AssetForKind(document, Doc::AssetKind::Scene3d);
    const std::string package = AssetForKind(document, Doc::AssetKind::Package2d);
    if (auto* draw = std::get_if<Doc::ModelDraw>(&command)) draw->asset = scene;
    if (auto* cell = std::get_if<Doc::SpriteDraw>(&command)) cell->asset = package;
    if (auto* animate = std::get_if<Doc::SpriteAnimate>(&command)) animate->asset = package;
    if (auto* emitter = std::get_if<Doc::EmitterCmd>(&command)) emitter->asset = package;
}

std::string Slug(std::string_view name) {
    std::string out;
    for (const char c : name)
        out += (c == '.') ? '_' : c;
    return out;
}

std::string UniqueTrackId(const Doc::Document& document, std::string_view base) {
    std::string candidate(base);
    if (candidate.empty()) candidate = "track";
    for (int counter = 2; FindTrack(document, candidate) >= 0 && counter < 10000; counter++)
        candidate = std::string(base) + "_" + std::to_string(counter);
    return candidate;
}

}

std::vector<PaletteEntry> PaletteEntries(const Doc::Document& document, std::string_view track_id,
                                         int frame, std::string_view filter) {
    const Doc::Track* track = TrackById(document, track_id);
    const std::string needle = Lower(filter);

    std::vector<PaletteEntry> out;
    for (std::size_t i = 0; i < kBlurbs.size(); i++) {
        const auto type = (Doc::CommandType)i;
        if (!Matches(kBlurbs[i], needle)) continue;
        PaletteEntry entry;
        entry.type = type;
        entry.section = SectionFor(type, track);
        entry.name = std::string(kBlurbs[i].name);
        entry.summary = std::string(kBlurbs[i].summary);
        if (entry.section == PaletteSection::Track) {
            const std::string blocker = PrimaryBlocker(document, track_id, type, frame,
                                                       DurationEnd(document, track, type, frame));
            if (!blocker.empty()) {
                entry.enabled = false;
                entry.refusal = "refused at " + std::to_string(frame) + ": overlaps " + blocker;
            }
        }
        out.push_back(std::move(entry));
    }

    std::ranges::stable_sort(
        out, [](const PaletteEntry& a, const PaletteEntry& b) { return a.section < b.section; });
    return out;
}

std::string InsertPaletteCommand(Doc::Document& document, std::string_view track_id,
                                 Doc::CommandType type, int frame) {
    const Doc::TrackKind kind = Doc::TraitsFor(type).kind;
    const Doc::Track* selected = TrackById(document, track_id);
    std::string home(track_id);
    if (selected == nullptr || selected->kind != kind) {
        home = InsertTrack(document,
                           TrackSpec{.kind = kind,
                                     .name = std::string(Doc::kTrackKindNames[(std::size_t)kind]),
                                     .below_selected = true},
                           track_id);
    }
    const Doc::Track* track = TrackById(document, home);
    if (track == nullptr) return {};

    Doc::Clip clip;
    clip.id = UniqueClipId(document, home + "_" + Slug(Doc::kCommandTypeNames[(std::size_t)type]));
    clip.start = std::max(0, frame);
    clip.end = DurationEnd(document, track, type, clip.start);
    clip.command = Doc::DefaultCommand(type);
    FillAsset(document, clip.command);

    const std::string id = clip.id;
    document.tracks[(std::size_t)FindTrack(document, home)].clips.push_back(std::move(clip));
    return id;
}

std::string InsertTrack(Doc::Document& document, const TrackSpec& spec,
                        std::string_view selected_track_id) {
    Doc::Track track;
    const std::string base = spec.name.empty() ? spec.target : spec.name;
    track.id = UniqueTrackId(
        document, base.empty() ? std::string(Doc::kTrackKindNames[(std::size_t)spec.kind]) : base);
    track.name = base.empty() ? track.id : base;
    track.kind = spec.kind;
    if (Doc::HasTarget(spec.kind)) track.target = spec.target.empty() ? track.name : spec.target;

    const int selected = FindTrack(document, selected_track_id);
    const std::string id = track.id;
    if (selected < 0 || !spec.below_selected) {
        document.tracks.push_back(std::move(track));
        return id;
    }
    document.tracks.insert(document.tracks.begin() + selected + 1, std::move(track));
    return id;
}

}
