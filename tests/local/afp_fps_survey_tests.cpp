#include <catch2/catch_test_macros.hpp>

#include "document/document.h"
#include "document/outline.h"
#include "document/playback.h"
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
#include <vector>

namespace {

struct Counts {
    std::map<std::string, std::size_t> rates;
    std::size_t animations = 0;
    std::size_t fixed_point = 0;
    std::size_t floating = 0;
    std::size_t out_of_range = 0;
};

std::vector<uint8_t> ReadAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void CountNodes(const Document::File& file, const std::vector<Document::Node>& nodes,
                Counts& counts) {
    for (const Document::Node& node : nodes) {
        if (node.role == Document::Role::Animation) {
            const auto animation = file.ReadAnimation(node.path);
            if (animation) {
                counts.animations++;
                if ((animation->flags & 0x2) != 0) {
                    counts.fixed_point++;
                } else {
                    counts.floating++;
                }
                const double rate = Document::FrameRate(*animation);
                counts.rates[std::format("{:.3f}", rate)]++;
                if (rate < Document::kSlowestFrameRate || rate > Document::kFastestFrameRate)
                    counts.out_of_range++;
            }
        }
        CountNodes(file, node.children, counts);
    }
}

}

TEST_CASE("What frame rate a shipped animation asks to play at") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");

    Counts counts;
    std::size_t files = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir + "/data")) {
        if (!entry.is_regular_file() || entry.path().extension() != ".ifs") continue;
        auto file = Document::File::Open(ReadAll(entry.path()));
        if (!file) continue;
        files++;
        CountNodes(*file, file->Nodes(), counts);
    }

    std::cerr << std::format(
        "[fps] {} files, {} animations, {} fixed point, {} float, {} outside the playable range\n",
        files, counts.animations, counts.fixed_point, counts.floating, counts.out_of_range);
    std::size_t shown = 0;
    for (const auto& [rate, count] : counts.rates) {
        if (shown++ >= 20) break;
        std::cerr << std::format("[fps] {}: {}\n", rate, count);
    }

    CHECK(counts.animations > 0);
}
