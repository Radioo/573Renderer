#pragma once

#include "preset/doc/preset_document.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Editor {

struct ClipRef {
    int track = -1;
    int clip = -1;
    [[nodiscard]] bool Valid() const { return track >= 0 && clip >= 0; }
};

ClipRef FindClip(const Preset::Doc::Document& document, std::string_view clip_id);

int FindTrack(const Preset::Doc::Document& document, std::string_view track_id);

int DocumentLength(const Preset::Doc::Document& document);

bool IsEvent(const Preset::Doc::Clip& clip);

int ClipEnd(const Preset::Doc::Clip& clip, int length);

struct Span {
    int start = 0;
    int end = 0;
};

std::vector<Span> PrimaryPeers(const Preset::Doc::Document& document, std::string_view track_id,
                               std::string_view clip_id);

bool Overlaps(const Preset::Doc::Document& document, std::string_view track_id,
              std::string_view clip_id, int start, std::optional<int> end);

bool OverlapsSelfCopy(const Preset::Doc::Document& document, std::string_view track_id,
                      std::string_view clip_id, int start, std::optional<int> end);

std::string PrimaryBlocker(const Preset::Doc::Document& document, std::string_view track_id,
                           Preset::Doc::CommandType type, int start, std::optional<int> end);

std::string UniqueClipId(const Preset::Doc::Document& document, std::string_view base);

bool MoveClip(Preset::Doc::Document& document, std::string_view clip_id, int start);

bool MoveClipToTrack(Preset::Doc::Document& document, std::string_view clip_id,
                     std::string_view track_id, int start);

bool ResizeClip(Preset::Doc::Document& document, std::string_view clip_id, int start,
                std::optional<int> end);

bool SplitClip(Preset::Doc::Document& document, std::string_view clip_id, int frame);

bool TrimStart(Preset::Doc::Document& document, std::string_view clip_id, int frame);

bool TrimEnd(Preset::Doc::Document& document, std::string_view clip_id, int frame);

bool MakeOpenEnded(Preset::Doc::Document& document, std::string_view clip_id);

bool SetClipMuted(Preset::Doc::Document& document, std::string_view clip_id, bool muted);

bool DeleteClips(Preset::Doc::Document& document, const std::vector<std::string>& clip_ids);

struct ClipboardClip {
    std::string track_id = {};
    Preset::Doc::Clip clip = {};
};

std::vector<ClipboardClip> CopyClips(const Preset::Doc::Document& document,
                                     const std::vector<std::string>& clip_ids);

std::vector<std::string> PasteClips(Preset::Doc::Document& document,
                                    const std::vector<ClipboardClip>& clips, int frame);

std::vector<std::string> DuplicateClips(Preset::Doc::Document& document,
                                        const std::vector<std::string>& clip_ids);

bool DuplicateClipTo(Preset::Doc::Document& document, std::string_view clip_id,
                     std::string_view track_id, std::string new_id, int start);

bool SetTrackMuted(Preset::Doc::Document& document, std::string_view track_id, bool muted);

bool SetTrackSolo(Preset::Doc::Document& document, std::string_view track_id, bool solo);

bool SetTrackLocked(Preset::Doc::Document& document, std::string_view track_id, bool locked);

bool RenameTrack(Preset::Doc::Document& document, std::string_view track_id, std::string name);

bool MoveTrack(Preset::Doc::Document& document, std::string_view track_id, int delta);

bool DuplicateTrack(Preset::Doc::Document& document, std::string_view track_id);

bool DeleteTrack(Preset::Doc::Document& document, std::string_view track_id);

bool TrackLocked(const Preset::Doc::Document& document, std::string_view track_id);

bool AddMarker(Preset::Doc::Document& document, int frame, std::string label);

bool RenameMarker(Preset::Doc::Document& document, int index, std::string label);

bool MoveMarker(Preset::Doc::Document& document, int index, int frame);

bool DeleteMarker(Preset::Doc::Document& document, int index);

bool SetLength(Preset::Doc::Document& document, int length);

std::vector<int> EdgeFrames(const Preset::Doc::Document& document);

}
