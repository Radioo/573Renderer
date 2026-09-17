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
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace {

struct Counts {
    std::size_t files = 0;
    std::map<std::string, std::size_t> spans;
    std::map<std::string, std::size_t> filters;
    std::map<std::string, std::size_t> updates;
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

std::string Shape(const std::vector<AfpAnimation::Filter>& filters) {
    std::string shape;
    for (const AfpAnimation::Filter& filter : filters) {
        if (const auto* matrix = std::get_if<AfpAnimation::ColourMatrixFilter>(&filter)) {
            shape += matrix->hsv ? "matrix+hsv " : "matrix ";
        } else if (std::holds_alternative<AfpAnimation::LookupFilter>(filter)) {
            shape += "lookup ";
        } else {
            shape += "unknown ";
        }
    }
    return shape.empty() ? "empty" : shape;
}

bool SameShape(const std::vector<AfpAnimation::Filter>& a,
               const std::vector<AfpAnimation::Filter>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); i++) {
        if (a[i].index() != b[i].index()) return false;
        const auto* left = std::get_if<AfpAnimation::ColourMatrixFilter>(&a[i]);
        const auto* right = std::get_if<AfpAnimation::ColourMatrixFilter>(&b[i]);
        if (left != nullptr && right != nullptr &&
            (left->head != right->head || left->hsv.has_value() != right->hsv.has_value()))
            return false;
        if (left == nullptr && a[i] != b[i]) return false;
    }
    return true;
}

void CountMatrixChanges(const std::vector<AfpAnimation::Filter>& a,
                        const std::vector<AfpAnimation::Filter>& b, Counts& counts) {
    for (std::size_t i = 0; i < a.size() && i < b.size(); i++) {
        const auto* left = std::get_if<AfpAnimation::ColourMatrixFilter>(&a[i]);
        const auto* right = std::get_if<AfpAnimation::ColourMatrixFilter>(&b[i]);
        if (left == nullptr || right == nullptr) continue;
        for (std::size_t j = 0; j < left->matrix.size(); j++) {
            if (left->matrix.at(j) != right->matrix.at(j))
                counts.updates[std::format("matrix entry {} changes", j)]++;
        }
        if (left->hsv != right->hsv) counts.updates["hsv changes"]++;
    }
}

void CountSpan(const AfpAnimation::Container& clip, uint16_t depth, uint32_t first, uint32_t last,
               Counts& counts) {
    const auto placements = SpanPlacements(clip, depth, first, last);
    if (placements.size() < 2 || (placements.front()->flags & 1U) != 0) return;
    const std::optional<std::vector<AfpAnimation::Filter>>& created = placements.front()->filters;
    std::size_t carrying = 0;
    std::size_t same = 0;
    std::size_t same_shape = 0;
    std::optional<std::vector<AfpAnimation::Filter>> previous = created;
    for (std::size_t i = 1; i < placements.size(); i++) {
        const auto& filters = placements[i]->filters;
        if (!filters) continue;
        carrying++;
        counts.filters[Shape(*filters)]++;
        if (filters == created) same++;
        if (created && SameShape(*filters, *created)) same_shape++;
        if (previous) CountMatrixChanges(*previous, *filters, counts);
        previous = filters;
    }
    if (carrying == 0) return;
    const std::size_t updates = placements.size() - 1;
    std::string kind = carrying == updates ? "every update" : "some updates";
    kind += created ? ", create has filters" : ", create has none";
    if (same == carrying) {
        kind += ", all equal to the create's";
    } else if (same_shape == carrying) {
        kind += ", same shape as the create's, numbers change";
    } else {
        kind += ", shape changes";
    }
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

TEST_CASE("How spans whose updates carry filters use them") {
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

    std::cerr << std::format("[filter spans] {} files\n", counts.files);
    for (const auto& [what, count] : counts.spans)
        std::cerr << std::format("[filter spans] span: {}: {}\n", what, count);
    for (const auto& [what, count] : counts.filters)
        std::cerr << std::format("[filter spans] update filters {}: {}\n", what, count);
    for (const auto& [what, count] : counts.updates)
        std::cerr << std::format("[filter spans] between updates, {}: {}\n", what, count);
    CHECK(counts.files > 0);
}
