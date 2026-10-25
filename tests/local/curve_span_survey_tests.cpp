#include <catch2/catch_test_macros.hpp>

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
#include <variant>
#include <vector>

namespace {

constexpr std::size_t kControlPoints = 0x10;

struct Counts {
    std::size_t files = 0;
    std::map<std::string, std::size_t> sets;
    std::map<std::string, std::size_t> spans;
    std::map<std::string, std::size_t> later;
};

std::vector<uint8_t> ReadAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::vector<const AfpAnimation::Placement*>
SpanPlacements(const AfpAnimation::Container& clip, uint16_t depth, uint32_t first, uint32_t last) {
    std::vector<const AfpAnimation::Placement*> found;
    for (uint32_t frame = first; frame <= last && frame < clip.frames.size(); frame++) {
        const AfpAnimation::Frame& owner = clip.frames[frame];
        for (uint32_t i = 0; i < owner.tag_count; i++) {
            const std::size_t index = owner.first_tag + i;
            if (index >= clip.tags.size()) break;
            const auto* placement = std::get_if<AfpAnimation::Placement>(&clip.tags[index].body);
            if (placement != nullptr && placement->depth == depth) found.push_back(placement);
        }
    }
    return found;
}

std::size_t Points(const AfpAnimation::Curve& curve) {
    return curve.values.size() / ((curve.flags & kControlPoints) != 0 ? 6 : 2);
}

void CountSet(const std::vector<AfpAnimation::Curve>& curves, Counts& counts) {
    bool packed = true;
    for (std::size_t i = 0; i < curves.size(); i++)
        packed = packed && curves[i].slot == i;
    counts.sets[packed ? "slots packed from 0" : "slots with gaps"]++;
    counts.sets[std::format("{} curves", curves.size())]++;
    for (const AfpAnimation::Curve& curve : curves) {
        counts.sets[std::format("flags {:04x}", curve.flags)]++;
        counts.sets[std::format("{} points", Points(curve))]++;
    }
}

std::string CompareLater(const std::vector<AfpAnimation::Curve>& first,
                         const std::vector<AfpAnimation::Curve>& later) {
    if (later == first) return "same as the first set";
    for (const AfpAnimation::Curve& curve : later) {
        if (curve.slot >= first.size()) return "names a slot past the first set's count";
        if (Points(curve) > Points(first[curve.slot])) return "more points than the first set";
    }
    for (const AfpAnimation::Curve& curve : later) {
        if (Points(curve) < Points(first[curve.slot])) return "fewer points in some slot";
        if (curve.flags != first[curve.slot].flags) return "other flags in some slot";
    }
    if (later.size() < first.size()) return "fewer curves, same points";
    return "same slots and points, values change";
}

void CountSpan(const AfpAnimation::Container& clip, uint16_t depth, uint32_t first, uint32_t last,
               Counts& counts) {
    const auto placements = SpanPlacements(clip, depth, first, last);
    const AfpAnimation::Placement* opener = nullptr;
    std::size_t carrying = 0;
    for (const AfpAnimation::Placement* placement : placements) {
        if (!placement->curves) continue;
        CountSet(*placement->curves, counts);
        if (opener == nullptr) {
            opener = placement;
            continue;
        }
        carrying++;
        counts.later[CompareLater(*opener->curves, *placement->curves)]++;
    }
    if (opener == nullptr) return;
    std::string kind =
        (opener->flags & 1U) != 0 ? "first set on an update" : "first set on the create";
    if (opener->curves && opener->curves->empty()) kind += ", empty";
    kind += carrying == 0 ? ", no later sets" : ", later sets";
    counts.spans[kind]++;
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
            if (animation) CountContainer(animation->root, counts);
        }
        CountNodes(file, node.children, counts);
    }
}

}

TEST_CASE("How spans use deformation curves") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");

    Counts counts;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir + "/data")) {
        if (!entry.is_regular_file() || entry.path().extension() != ".ifs") continue;
        const auto file = Document::File::Open(ReadAll(entry.path()));
        if (!file) continue;
        counts.files++;
        CountNodes(*file, file->Nodes(), counts);
    }

    std::cerr << std::format("[curve spans] {} files\n", counts.files);
    for (const auto& [what, count] : counts.spans)
        std::cerr << std::format("[curve spans] span: {}: {}\n", what, count);
    for (const auto& [what, count] : counts.sets)
        std::cerr << std::format("[curve spans] set: {}: {}\n", what, count);
    for (const auto& [what, count] : counts.later)
        std::cerr << std::format("[curve spans] later set: {}: {}\n", what, count);
    CHECK(counts.files > 0);
}
