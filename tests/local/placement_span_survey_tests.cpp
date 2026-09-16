#include <catch2/catch_test_macros.hpp>

#include "document/authored.h"
#include "document/clip.h"
#include "document/document.h"
#include "document/outline.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "support/env.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

struct OwnCounts {
    std::map<std::string, std::size_t> refusals;
    std::map<std::string, std::size_t> differences;
    std::size_t owned = 0;
    std::size_t tried = 0;
    std::size_t detach_exact = 0;
    std::size_t detach_differs = 0;
};

struct Counts {
    std::map<std::string, std::size_t> masks;
    OwnCounts root_own;
    OwnCounts sprite_own;
    std::size_t spans = 0;
    std::size_t one_frame = 0;
    std::size_t steady_updates = 0;
    std::size_t changing_updates = 0;
    std::size_t create_only = 0;
    std::size_t extra_creates = 0;
    std::size_t placements = 0;
    std::size_t updates = 0;
};

std::vector<uint8_t> ReadAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::string Mask(const AfpAnimation::Placement& placement) {
    return std::format("{:08x}/{:08x}", placement.flags & ~1U,
                       placement.extended_flags.value_or(0));
}

void CountSpan(const AfpAnimation::Container& clip, uint16_t depth, uint32_t first, uint32_t last,
               Counts& counts) {
    counts.spans++;
    if (first == last) counts.one_frame++;
    std::vector<const AfpAnimation::Placement*> placements;
    for (uint32_t frame = first; frame <= last && frame < clip.frames.size(); frame++) {
        const AfpAnimation::Frame& owner = clip.frames[frame];
        for (uint32_t i = 0; i < owner.tag_count; i++) {
            const std::size_t index = owner.first_tag + i;
            if (index >= clip.tags.size()) break;
            const auto* placement = std::get_if<AfpAnimation::Placement>(&clip.tags[index].body);
            if (placement != nullptr && placement->depth == depth) placements.push_back(placement);
        }
    }
    if (placements.empty()) return;

    std::size_t creates = 0;
    std::string first_update;
    bool steady = true;
    for (const AfpAnimation::Placement* placement : placements) {
        counts.placements++;
        if ((placement->flags & 1U) == 0) {
            creates++;
            continue;
        }
        counts.updates++;
        const std::string mask = Mask(*placement);
        counts.masks[mask]++;
        if (first_update.empty()) {
            first_update = mask;
        } else if (mask != first_update) {
            steady = false;
        }
    }
    if (creates > 1) counts.extra_creates++;
    if (first_update.empty()) {
        counts.create_only++;
    } else if (steady) {
        counts.steady_updates++;
    } else {
        counts.changing_updates++;
    }
}

std::string Reason(const std::string& error) {
    if (error.find("which is not something a keyframe can hold") != std::string::npos)
        return error.substr(error.find("changes "));
    if (error.find("again instead of updating") != std::string::npos)
        return "placed again inside its span";
    if (error.find("somewhere else than its first frame") != std::string::npos)
        return "an end frame of its own";
    if (error.find("with flags of its own") != std::string::npos)
        return "an update with flags of its own";
    if (error.find("does not start with a placement") != std::string::npos)
        return "no placement on its first frame";
    return error;
}

std::vector<std::pair<uint32_t, const AfpAnimation::Placement*>>
AtDepth(const AfpAnimation::Container& clip, uint16_t depth) {
    std::vector<std::pair<uint32_t, const AfpAnimation::Placement*>> found;
    for (uint32_t frame = 0; frame < clip.frames.size(); frame++) {
        const AfpAnimation::Frame& owner = clip.frames[frame];
        for (uint32_t i = 0; i < owner.tag_count; i++) {
            const std::size_t index = owner.first_tag + i;
            if (index >= clip.tags.size()) break;
            const auto* placement = std::get_if<AfpAnimation::Placement>(&clip.tags[index].body);
            if (placement != nullptr && placement->depth == depth)
                found.emplace_back(frame, placement);
        }
    }
    return found;
}

std::string TransformDifference(const AfpAnimation::Placement& a,
                                const AfpAnimation::Placement& b) {
    if (a.flags != b.flags) return "flags";
    if (a.extended_flags != b.extended_flags) return "extended flags";
    if (a.end_frame != b.end_frame) return "end frame";
    if (a.character != b.character) return "character";
    if (a.ratio != b.ratio) return "ratio";
    if (a.name != b.name) return "name";
    if (a.clip_depth != b.clip_depth) return "clip depth";
    if (a.blend != b.blend) return "blend";
    if (a.scale != b.scale) return "scale";
    if (a.rotate_skew != b.rotate_skew) return "rotate skew";
    if (a.translation != b.translation) return "translation";
    if (a.multiply_colour != b.multiply_colour) return "multiply colour";
    if (a.add_colour != b.add_colour) return "add colour";
    if (a.packed_multiply_colour != b.packed_multiply_colour) return "packed multiply colour";
    if (a.packed_add_colour != b.packed_add_colour) return "packed add colour";
    return {};
}

std::string ExtraDifference(const AfpAnimation::Placement& a, const AfpAnimation::Placement& b) {
    if (a.clip_actions != b.clip_actions) return "clip actions";
    if (a.filters != b.filters) return "filters";
    if (a.origin != b.origin) return "origin";
    if (a.origin_z != b.origin_z) return "origin z";
    if (a.geometry != b.geometry) return "geometry";
    if (a.short_scale != b.short_scale) return "short scale";
    if (a.short_rotate_skew != b.short_rotate_skew) return "short rotate skew";
    if (a.class_name != b.class_name) return "class name";
    if (a.translation_z != b.translation_z) return "translation z";
    if (a.matrix_3d != b.matrix_3d) return "3D matrix";
    if (a.hsv != b.hsv) return "HSV";
    if (a.discarded_words != b.discarded_words) return "discarded words";
    if (a.curves != b.curves) return "curves";
    if (a.colour_controller != b.colour_controller) return "colour controller";
    if (a.grid_controller != b.grid_controller) return "grid controller";
    return {};
}

std::string PlacementDifference(const AfpAnimation::Placement& a,
                                const AfpAnimation::Placement& b) {
    const std::string first = TransformDifference(a, b);
    return first.empty() ? ExtraDifference(a, b) : first;
}

std::string Difference(const AfpAnimation::Container& before, const AfpAnimation::Container& after,
                       uint16_t depth) {
    if (before.tags.size() != after.tags.size()) return "the clip holds a different tag count";
    if (before.frames != after.frames) return "the frame ranges moved";
    const auto old_ones = AtDepth(before, depth);
    const auto new_ones = AtDepth(after, depth);
    if (old_ones.size() != new_ones.size()) return "a different number of placements at the depth";
    for (std::size_t i = 0; i < old_ones.size(); i++) {
        if (old_ones[i].first != new_ones[i].first) return "a placement landed on another frame";
        const AfpAnimation::Placement& a = *old_ones[i].second;
        const AfpAnimation::Placement& b = *new_ones[i].second;
        const std::string what = PlacementDifference(a, b);
        if (!what.empty()) return what;
    }
    return "something outside the depth";
}

void CountOwn(const AfpAnimation::Animation& animation, Document::ClipId clip, uint16_t depth,
              uint32_t frame, OwnCounts& counts) {
    counts.tried++;
    const auto authored = Document::OwnDepth(animation, clip, "afp", depth, frame);
    if (!authored) {
        counts.refusals[Reason(authored.error())]++;
        return;
    }
    counts.owned++;
    AfpAnimation::Animation again = animation;
    const auto detached = Document::WriteAuthored(again, authored->authored, authored->baked);
    if (!detached) {
        counts.refusals["detach: " + detached.error()]++;
        return;
    }
    if (again == animation) {
        counts.detach_exact++;
        return;
    }
    counts.detach_differs++;
    const AfpAnimation::Container* before = Document::FindClip(animation, clip);
    const AfpAnimation::Container* after = Document::FindClip(again, clip);
    if (before == nullptr || after == nullptr) {
        counts.differences["the clip itself"]++;
        return;
    }
    counts.differences[Difference(*before, *after, depth)]++;
}

void CountOwnSpans(const AfpAnimation::Animation& animation, Document::ClipId clip,
                   OwnCounts& counts) {
    const AfpAnimation::Container* found = Document::FindClip(animation, clip);
    if (found == nullptr) return;
    for (const Document::DepthRow& row : Document::DepthRows(*found)) {
        for (const Document::Span& span : row.spans)
            CountOwn(animation, clip, row.depth, span.first_frame, counts);
    }
}

void ReportOwn(const std::string& scope, const OwnCounts& counts) {
    std::cerr << std::format("[own {}] {} tried, {} owned, {} detach exact, {} detach differs\n",
                             scope, counts.tried, counts.owned, counts.detach_exact,
                             counts.detach_differs);
    for (const auto& [what, count] : counts.differences)
        std::cerr << std::format("[own {}] detach differs in {}: {}\n", scope, what, count);
    std::size_t shown = 0;
    for (const auto& [reason, count] : counts.refusals) {
        if (shown++ >= 20) break;
        std::cerr << std::format("[own {}] refused, {}: {}\n", scope, reason, count);
    }
}

void CountContainer(const AfpAnimation::Container& clip, Counts& counts) {
    for (const Document::DepthRow& row : Document::DepthRows(clip)) {
        for (const Document::Span& span : row.spans)
            CountSpan(clip, row.depth, span.first_frame, span.last_frame, counts);
    }
    for (const AfpAnimation::Tag& tag : clip.tags) {
        if (const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body))
            CountContainer(sprite->container, counts);
    }
}

void CountNodes(const Document::File& file, const std::vector<Document::Node>& nodes,
                Counts& counts) {
    for (const Document::Node& node : nodes) {
        if (node.role == Document::Role::Animation) {
            const auto animation = file.ReadAnimation(node.path);
            if (animation) {
                CountContainer(animation->root, counts);
                CountOwnSpans(*animation, Document::ClipId{}, counts.root_own);
                for (const Document::ClipSummary& clip : Document::Clips(*animation)) {
                    if (clip.id.sprite) CountOwnSpans(*animation, clip.id, counts.sprite_own);
                }
            }
        }
        CountNodes(file, node.children, counts);
    }
}

void Walk(const std::string& dir, Counts& counts, std::size_t& files) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir + "/data")) {
        if (!entry.is_regular_file() || entry.path().extension() != ".ifs") continue;
        auto file = Document::File::Open(ReadAll(entry.path()));
        if (!file) continue;
        files++;
        CountNodes(*file, file->Nodes(), counts);
    }
}

}

TEST_CASE("What a per frame placement carries over a span") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");

    Counts counts;
    std::size_t files = 0;
    Walk(dir, counts, files);

    std::cerr << std::format("[placement spans] {} files, {} spans, {} placements, {} updates\n",
                             files, counts.spans, counts.placements, counts.updates);
    std::cerr << std::format(
        "[placement spans] {} one frame, {} create only, {} steady updates, {} changing updates, "
        "{} with more than one create\n",
        counts.one_frame, counts.create_only, counts.steady_updates, counts.changing_updates,
        counts.extra_creates);
    std::size_t shown = 0;
    for (const auto& [mask, count] : counts.masks) {
        if (shown++ >= 20) break;
        std::cerr << std::format("[placement spans] update mask {}: {}\n", mask, count);
    }

    ReportOwn("root", counts.root_own);
    ReportOwn("sprite", counts.sprite_own);

    CHECK(counts.spans > 0);
    CHECK(counts.root_own.detach_differs == 0);
    CHECK(counts.sprite_own.detach_differs == 0);
}
