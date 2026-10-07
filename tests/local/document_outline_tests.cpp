#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/outline.h"
#include "formats/ifs_archive.h"
#include "support/env.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kTitleFrames = 840;
constexpr uint32_t kLoopFrame = 240;

std::vector<uint8_t> ReadAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}

TEST_CASE("The outline names and describes a shipped package") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");

    const std::vector<uint8_t> bytes = ReadAll(dir + "/data/graphic/1/title.ifs");
    REQUIRE(!bytes.empty());
    const auto archive = Ifs::Read(bytes);
    const std::string read_error = archive.has_value() ? std::string() : archive.error();
    INFO(read_error);
    REQUIRE(archive.has_value());

    const Document::Outline outline = Document::Outline::Build(*archive);
    CHECK(outline.Problems().empty());

    const auto animations = std::ranges::find(outline.Nodes(), "afp", &Document::Node::path);
    REQUIRE(animations != outline.Nodes().end());
    const auto title = std::ranges::find(animations->children, "title", &Document::Node::name);
    REQUIRE(title != animations->children.end());
    CHECK(title->role == Document::Role::Animation);

    const auto details = outline.Describe(*archive, title->path);
    const std::string describe_error = details.has_value() ? std::string() : details.error();
    INFO(describe_error);
    REQUIRE(details.has_value());
    REQUIRE(details->animation.has_value());
    const Document::AnimationDetails animation =
        details->animation.value_or(Document::AnimationDetails{});
    CHECK(animation.frame_count == kTitleFrames);
    REQUIRE(animation.labels.size() == 1);
    CHECK(animation.labels[0].name == "loop");
    CHECK(animation.labels[0].frame == kLoopFrame);

    const auto textures = std::ranges::find(outline.Nodes(), "tex", &Document::Node::path);
    REQUIRE(textures != outline.Nodes().end());
    const auto image = std::ranges::find_if(textures->children, [](const Document::Node& node) {
        return node.role == Document::Role::Texture;
    });
    REQUIRE(image != textures->children.end());
    const auto texture = outline.Describe(*archive, image->path);
    REQUIRE(texture.has_value());
    REQUIRE(texture->texture.has_value());
    const Document::TextureDetails image_details =
        texture->texture.value_or(Document::TextureDetails{});
    CHECK(image_details.format == "argb8888rev");
    CHECK(image_details.width > 0);
}
