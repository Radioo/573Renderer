#include "editor/timeline_edits.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_fields.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Editor {

namespace Doc = Preset::Doc;

namespace {

Doc::Clip* MutClip(Doc::Document& document, std::string_view clip_id) {
    const ClipRef ref = FindClip(document, clip_id);
    if (!ref.Valid()) return nullptr;
    return &document.tracks[(std::size_t)ref.track].clips[(std::size_t)ref.clip];
}

Doc::Track* MutTrack(Doc::Document& document, std::string_view track_id) {
    const int index = FindTrack(document, track_id);
    if (index < 0) return nullptr;
    return &document.tracks[(std::size_t)index];
}

bool Intersects(int a_start, int a_end, int b_start, int b_end) {
    return a_start < b_end && b_start < a_end;
}

bool Disjoint(const std::vector<std::string>& a, const std::vector<std::string>& b) {
    return std::ranges::none_of(
        a, [&b](const std::string& value) { return std::ranges::find(b, value) != b.end(); });
}

bool Contains(const std::vector<std::string>& outer, const std::vector<std::string>& inner) {
    return std::ranges::all_of(inner, [&outer](const std::string& value) {
        return std::ranges::find(outer, value) != outer.end();
    });
}

bool GatesExclusive(const std::optional<Doc::Gate>& a, const std::optional<Doc::Gate>& b) {
    if (!a.has_value() || !b.has_value()) return false;
    if (a->option != b->option) return false;
    const bool a_not = a->kind == Doc::GateKind::Not;
    const bool b_not = b->kind == Doc::GateKind::Not;
    if (a_not && b_not) return false;
    if (a_not) return Contains(a->choices, b->choices);
    if (b_not) return Contains(b->choices, a->choices);
    return Disjoint(a->choices, b->choices);
}

int LightIndex(const Doc::Command& command) {
    const auto* light = std::get_if<Doc::LightSet>(&command);
    return light != nullptr ? light->index : 0;
}

bool SameSlot(const Doc::Command& a, const Doc::Command& b) {
    const Doc::Family family = Doc::TraitsFor(Doc::TypeOf(a)).family;
    if (family != Doc::TraitsFor(Doc::TypeOf(b)).family) return false;
    if (family == Doc::Family::Light) return LightIndex(a) == LightIndex(b);
    return true;
}

bool SharesTarget(const Doc::Track& a, const Doc::Track& b) {
    if (!Doc::HasTarget(a.kind)) return false;
    return a.kind == b.kind && a.target == b.target;
}

std::string SuffixedId(std::string_view base, int counter) {
    std::string id(base);
    id += "_";
    id += std::to_string(counter);
    return id;
}

std::string TrackForKind(const Doc::Document& document, Doc::TrackKind kind) {
    for (const Doc::Track& track : document.tracks) {
        if (track.kind == kind && !track.locked) return track.id;
    }
    return {};
}

void SortMarkers(Doc::Document& document) {
    std::ranges::stable_sort(document.markers, [](const Doc::Marker& a, const Doc::Marker& b) {
        return a.frame < b.frame;
    });
}

}

ClipRef FindClip(const Doc::Document& document, std::string_view clip_id) {
    for (std::size_t t = 0; t < document.tracks.size(); t++) {
        const std::vector<Doc::Clip>& clips = document.tracks[t].clips;
        for (std::size_t c = 0; c < clips.size(); c++) {
            if (clips[c].id == clip_id) return ClipRef{.track = (int)t, .clip = (int)c};
        }
    }
    return {};
}

int FindTrack(const Doc::Document& document, std::string_view track_id) {
    for (std::size_t t = 0; t < document.tracks.size(); t++) {
        if (document.tracks[t].id == track_id) return (int)t;
    }
    return -1;
}

bool IsEvent(const Doc::Clip& clip) {
    return Doc::TraitsFor(Doc::TypeOf(clip.command)).event;
}

int ClipEnd(const Doc::Clip& clip, int length) {
    if (IsEvent(clip)) return clip.start + 1;
    if (clip.end.has_value()) return *clip.end;
    return std::max(length, clip.start + 1);
}

int DocumentLength(const Doc::Document& document) {
    if (document.length.has_value() && *document.length > 0) return *document.length;
    int longest = 1;
    for (const Doc::Track& track : document.tracks) {
        for (const Doc::Clip& clip : track.clips)
            longest = std::max(longest, clip.end.value_or(clip.start + 1));
    }
    return longest;
}

std::vector<Span> PrimaryPeers(const Doc::Document& document, std::string_view track_id,
                               std::string_view clip_id) {
    std::vector<Span> peers;
    const ClipRef ref = FindClip(document, clip_id);
    const int home = FindTrack(document, track_id);
    if (!ref.Valid() || home < 0) return peers;
    const Doc::Clip& moved = document.tracks[(std::size_t)ref.track].clips[(std::size_t)ref.clip];
    if (Doc::TraitsFor(Doc::TypeOf(moved.command)).family == Doc::Family::None) return peers;

    const Doc::Track& target = document.tracks[(std::size_t)home];
    const int length = DocumentLength(document);
    for (const Doc::Track& track : document.tracks) {
        if (track.id != target.id && !SharesTarget(track, target)) continue;
        for (const Doc::Clip& other : track.clips) {
            if (other.id == clip_id) continue;
            if (!SameSlot(other.command, moved.command)) continue;
            if (GatesExclusive(moved.when, other.when)) continue;
            peers.push_back(Span{.start = other.start, .end = ClipEnd(other, length)});
        }
    }
    std::ranges::sort(peers, [](const Span& a, const Span& b) { return a.start < b.start; });
    return peers;
}

bool Overlaps(const Doc::Document& document, std::string_view track_id, std::string_view clip_id,
              int start, std::optional<int> end) {
    const ClipRef ref = FindClip(document, clip_id);
    if (!ref.Valid()) return false;
    const Doc::Clip& moved = document.tracks[(std::size_t)ref.track].clips[(std::size_t)ref.clip];
    const int length = DocumentLength(document);
    const int moved_end = IsEvent(moved) ? start + 1 : end.value_or(std::max(length, start + 1));
    const std::vector<Span> peers = PrimaryPeers(document, track_id, clip_id);
    return std::ranges::any_of(peers, [start, moved_end](const Span& peer) {
        return Intersects(start, moved_end, peer.start, peer.end);
    });
}

bool OverlapsSelfCopy(const Doc::Document& document, std::string_view track_id,
                      std::string_view clip_id, int start, std::optional<int> end) {
    if (Overlaps(document, track_id, clip_id, start, end)) return true;
    const ClipRef ref = FindClip(document, clip_id);
    const int index = FindTrack(document, track_id);
    if (!ref.Valid() || index < 0) return false;
    const Doc::Track& home = document.tracks[(std::size_t)ref.track];
    const Doc::Clip& source = home.clips[(std::size_t)ref.clip];
    if (Doc::TraitsFor(Doc::TypeOf(source.command)).family == Doc::Family::None) return false;
    const Doc::Track& destination = document.tracks[(std::size_t)index];
    if (destination.id != home.id && !SharesTarget(destination, home)) return false;
    const int length = DocumentLength(document);
    const int copy_end = IsEvent(source) ? start + 1 : end.value_or(std::max(length, start + 1));
    return Intersects(start, copy_end, source.start, ClipEnd(source, length));
}

std::string PrimaryBlocker(const Doc::Document& document, std::string_view track_id,
                           Doc::CommandType type, int start, std::optional<int> end) {
    const int home = FindTrack(document, track_id);
    if (home < 0 || Doc::TraitsFor(type).family == Doc::Family::None) return {};
    const Doc::Track& target = document.tracks[(std::size_t)home];
    const int length = DocumentLength(document);
    const int wanted_end =
        Doc::IsEvent(type) ? start + 1 : end.value_or(std::max(length, start + 1));
    for (const Doc::Track& track : document.tracks) {
        if (track.id != target.id && !SharesTarget(track, target)) continue;
        for (const Doc::Clip& other : track.clips) {
            if (!SameSlot(other.command, Doc::DefaultCommand(type))) continue;
            if (Intersects(start, wanted_end, other.start, ClipEnd(other, length))) return other.id;
        }
    }
    return {};
}

std::string UniqueClipId(const Doc::Document& document, std::string_view base) {
    if (!FindClip(document, base).Valid()) return std::string(base);
    for (int counter = 2; counter < 10000; counter++) {
        const std::string candidate = SuffixedId(base, counter);
        if (!FindClip(document, candidate).Valid()) return candidate;
    }
    return std::string(base);
}

bool MoveClip(Doc::Document& document, std::string_view clip_id, int start) {
    Doc::Clip* clip = MutClip(document, clip_id);
    if (clip == nullptr || start < 0) return false;
    if (clip->start == start) return false;
    if (clip->end.has_value()) *clip->end += start - clip->start;
    clip->start = start;
    return true;
}

bool MoveClipToTrack(Doc::Document& document, std::string_view clip_id, std::string_view track_id,
                     int start) {
    const ClipRef ref = FindClip(document, clip_id);
    const int destination = FindTrack(document, track_id);
    if (!ref.Valid() || destination < 0 || start < 0) return false;
    if (document.tracks[(std::size_t)ref.track].id == track_id) {
        return MoveClip(document, clip_id, start);
    }

    Doc::Track& from = document.tracks[(std::size_t)ref.track];
    Doc::Clip moved = from.clips[(std::size_t)ref.clip];
    if (moved.end.has_value()) *moved.end += start - moved.start;
    moved.start = start;
    from.clips.erase(from.clips.begin() + ref.clip);
    document.tracks[(std::size_t)destination].clips.push_back(std::move(moved));
    return true;
}

bool ResizeClip(Doc::Document& document, std::string_view clip_id, int start,
                std::optional<int> end) {
    Doc::Clip* clip = MutClip(document, clip_id);
    if (clip == nullptr || start < 0) return false;
    if (end.has_value() && *end <= start) return false;
    if (clip->start == start && clip->end == end) return false;
    clip->start = start;
    clip->end = end;
    return true;
}

bool SplitClip(Doc::Document& document, std::string_view clip_id, int frame) {
    const ClipRef ref = FindClip(document, clip_id);
    if (!ref.Valid()) return false;
    Doc::Track& track = document.tracks[(std::size_t)ref.track];
    Doc::Clip& clip = track.clips[(std::size_t)ref.clip];
    if (IsEvent(clip)) return false;
    const int length = DocumentLength(document);
    const int finish = ClipEnd(clip, length);
    if (frame <= clip.start || frame >= finish) return false;

    Doc::Clip right = clip;
    right.id = UniqueClipId(document, clip.id);
    right.start = frame;
    right.end = clip.end;
    right.keys.clear();
    for (const Doc::Key& key : clip.keys) {
        if (key.at + clip.start >= frame) {
            Doc::Key shifted = key;
            shifted.at = key.at + clip.start - frame;
            right.keys.push_back(std::move(shifted));
        }
    }
    clip.end = frame;
    std::erase_if(clip.keys, [&](const Doc::Key& key) { return key.at + clip.start >= frame; });
    track.clips.push_back(std::move(right));
    return true;
}

bool TrimStart(Doc::Document& document, std::string_view clip_id, int frame) {
    const ClipRef ref = FindClip(document, clip_id);
    if (!ref.Valid()) return false;
    const Doc::Clip& clip = document.tracks[(std::size_t)ref.track].clips[(std::size_t)ref.clip];
    if (IsEvent(clip) || frame < 0 || frame >= ClipEnd(clip, DocumentLength(document)))
        return false;
    return ResizeClip(document, clip_id, frame, clip.end);
}

bool TrimEnd(Doc::Document& document, std::string_view clip_id, int frame) {
    const ClipRef ref = FindClip(document, clip_id);
    if (!ref.Valid()) return false;
    const Doc::Clip& clip = document.tracks[(std::size_t)ref.track].clips[(std::size_t)ref.clip];
    if (IsEvent(clip) || frame <= clip.start) return false;
    return ResizeClip(document, clip_id, clip.start, frame);
}

bool MakeOpenEnded(Doc::Document& document, std::string_view clip_id) {
    Doc::Clip* clip = MutClip(document, clip_id);
    if (clip == nullptr || IsEvent(*clip) || !clip->end.has_value()) return false;
    clip->end.reset();
    return true;
}

bool SetClipMuted(Doc::Document& document, std::string_view clip_id, bool muted) {
    Doc::Clip* clip = MutClip(document, clip_id);
    if (clip == nullptr || clip->muted == muted) return false;
    clip->muted = muted;
    return true;
}

bool DeleteClips(Doc::Document& document, const std::vector<std::string>& clip_ids) {
    bool changed = false;
    for (Doc::Track& track : document.tracks) {
        const std::size_t before = track.clips.size();
        std::erase_if(track.clips, [&clip_ids](const Doc::Clip& clip) {
            return std::ranges::find(clip_ids, clip.id) != clip_ids.end();
        });
        changed = changed || track.clips.size() != before;
    }
    return changed;
}

std::vector<ClipboardClip> CopyClips(const Doc::Document& document,
                                     const std::vector<std::string>& clip_ids) {
    std::vector<ClipboardClip> out;
    for (const Doc::Track& track : document.tracks) {
        for (const Doc::Clip& clip : track.clips) {
            if (std::ranges::find(clip_ids, clip.id) == clip_ids.end()) continue;
            out.push_back(ClipboardClip{.track_id = track.id, .clip = clip});
        }
    }
    return out;
}

std::vector<std::string> PasteClips(Doc::Document& document,
                                    const std::vector<ClipboardClip>& clips, int frame) {
    std::vector<std::string> pasted;
    if (clips.empty()) return pasted;
    int earliest = clips.front().clip.start;
    for (const ClipboardClip& entry : clips)
        earliest = std::min(earliest, entry.clip.start);

    for (const ClipboardClip& entry : clips) {
        const Doc::Clip& source = entry.clip;
        std::string track_id = entry.track_id;
        if (FindTrack(document, track_id) < 0)
            track_id = TrackForKind(document, Doc::TraitsFor(Doc::TypeOf(source.command)).kind);
        Doc::Track* track = MutTrack(document, track_id);
        if (track == nullptr || track->locked) continue;
        Doc::Clip copy = source;
        copy.id = UniqueClipId(document, source.id);
        copy.start = std::max(0, frame + (source.start - earliest));
        if (source.end.has_value()) *copy.end = copy.start + (*source.end - source.start);
        pasted.push_back(copy.id);
        track->clips.push_back(std::move(copy));
    }
    return pasted;
}

std::vector<std::string> DuplicateClips(Doc::Document& document,
                                        const std::vector<std::string>& clip_ids) {
    const int length = DocumentLength(document);
    std::vector<std::string> made;
    for (const std::string& id : clip_ids) {
        const ClipRef ref = FindClip(document, id);
        if (!ref.Valid()) continue;
        Doc::Track& track = document.tracks[(std::size_t)ref.track];
        if (track.locked) continue;
        const Doc::Clip source = track.clips[(std::size_t)ref.clip];
        Doc::Clip copy = source;
        copy.id = UniqueClipId(document, source.id);
        const int span = ClipEnd(source, length) - source.start;
        copy.start = source.start + span;
        if (source.end.has_value()) *copy.end = copy.start + span;
        made.push_back(copy.id);
        track.clips.push_back(std::move(copy));
    }
    return made;
}

bool DuplicateClipTo(Doc::Document& document, std::string_view clip_id, std::string_view track_id,
                     std::string new_id, int start) {
    const ClipRef ref = FindClip(document, clip_id);
    const int destination = FindTrack(document, track_id);
    if (!ref.Valid() || destination < 0 || start < 0) return false;
    if (new_id.empty() || FindClip(document, new_id).Valid()) return false;
    if (document.tracks[(std::size_t)destination].locked) return false;

    Doc::Clip copy = document.tracks[(std::size_t)ref.track].clips[(std::size_t)ref.clip];
    if (copy.end.has_value()) *copy.end += start - copy.start;
    copy.start = start;
    copy.id = std::move(new_id);
    document.tracks[(std::size_t)destination].clips.push_back(std::move(copy));
    return true;
}

bool SetTrackMuted(Doc::Document& document, std::string_view track_id, bool muted) {
    Doc::Track* track = MutTrack(document, track_id);
    if (track == nullptr || track->muted == muted) return false;
    track->muted = muted;
    return true;
}

bool SetTrackSolo(Doc::Document& document, std::string_view track_id, bool solo) {
    Doc::Track* track = MutTrack(document, track_id);
    if (track == nullptr || track->solo == solo) return false;
    track->solo = solo;
    return true;
}

bool SetTrackLocked(Doc::Document& document, std::string_view track_id, bool locked) {
    Doc::Track* track = MutTrack(document, track_id);
    if (track == nullptr || track->locked == locked) return false;
    track->locked = locked;
    return true;
}

bool RenameTrack(Doc::Document& document, std::string_view track_id, std::string name) {
    Doc::Track* track = MutTrack(document, track_id);
    if (track == nullptr || name.empty() || track->name == name) return false;
    track->name = std::move(name);
    return true;
}

bool MoveTrack(Doc::Document& document, std::string_view track_id, int delta) {
    const int index = FindTrack(document, track_id);
    if (index < 0 || delta == 0) return false;
    const int target = index + delta;
    if (target < 0 || std::cmp_greater_equal(target, document.tracks.size())) return false;
    std::swap(document.tracks[(std::size_t)index], document.tracks[(std::size_t)target]);
    return true;
}

bool DuplicateTrack(Doc::Document& document, std::string_view track_id) {
    const int index = FindTrack(document, track_id);
    if (index < 0) return false;
    Doc::Track copy = document.tracks[(std::size_t)index];
    for (int counter = 2; counter < 10000; counter++) {
        const std::string candidate = SuffixedId(track_id, counter);
        if (FindTrack(document, candidate) >= 0) continue;
        copy.id = candidate;
        break;
    }
    copy.name += " copy";
    for (Doc::Clip& clip : copy.clips)
        clip.id = UniqueClipId(document, clip.id + "_copy");
    document.tracks.insert(document.tracks.begin() + index + 1, std::move(copy));
    return true;
}

bool DeleteTrack(Doc::Document& document, std::string_view track_id) {
    const int index = FindTrack(document, track_id);
    if (index < 0) return false;
    document.tracks.erase(document.tracks.begin() + index);
    return true;
}

bool TrackLocked(const Doc::Document& document, std::string_view track_id) {
    const int index = FindTrack(document, track_id);
    return index >= 0 && document.tracks[(std::size_t)index].locked;
}

bool AddMarker(Doc::Document& document, int frame, std::string label) {
    if (frame < 0) return false;
    const bool taken = std::ranges::any_of(
        document.markers, [frame](const Doc::Marker& marker) { return marker.frame == frame; });
    if (taken) return false;
    document.markers.push_back(Doc::Marker{.frame = frame, .label = std::move(label)});
    SortMarkers(document);
    return true;
}

bool RenameMarker(Doc::Document& document, int index, std::string label) {
    if (index < 0 || std::cmp_greater_equal(index, document.markers.size())) return false;
    if (document.markers[(std::size_t)index].label == label) return false;
    document.markers[(std::size_t)index].label = std::move(label);
    return true;
}

bool MoveMarker(Doc::Document& document, int index, int frame) {
    if (index < 0 || std::cmp_greater_equal(index, document.markers.size()) || frame < 0)
        return false;
    if (document.markers[(std::size_t)index].frame == frame) return false;
    document.markers[(std::size_t)index].frame = frame;
    SortMarkers(document);
    return true;
}

bool DeleteMarker(Doc::Document& document, int index) {
    if (index < 0 || std::cmp_greater_equal(index, document.markers.size())) return false;
    document.markers.erase(document.markers.begin() + index);
    return true;
}

bool SetLength(Doc::Document& document, int length) {
    if (length < 1) return false;
    if (document.length.has_value() && *document.length == length) return false;
    document.length = length;
    return true;
}

std::vector<int> EdgeFrames(const Doc::Document& document) {
    const int length = DocumentLength(document);
    std::vector<int> frames{0, length - 1};
    for (const Doc::Marker& marker : document.markers)
        frames.push_back(marker.frame);
    for (const Doc::Track& track : document.tracks) {
        for (const Doc::Clip& clip : track.clips) {
            frames.push_back(clip.start);
            frames.push_back(ClipEnd(clip, length));
            for (const Doc::Key& key : clip.keys)
                frames.push_back(clip.start + key.at);
        }
    }
    std::ranges::sort(frames);
    const auto duplicates = std::ranges::unique(frames);
    frames.erase(duplicates.begin(), duplicates.end());
    return frames;
}

}
