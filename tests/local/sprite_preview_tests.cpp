#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/clip.h"
#include "document/document.h"
#include "document/outline.h"
#include "document/sprite_preview.h"
#include "preview/preview_client.h"
#include "support/env.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kTitleFrames = 840;

std::vector<uint8_t> ReadAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::string AnimationPath(const Document::File& file) {
    for (const Document::Node& node : file.Nodes()) {
        for (const Document::Node& child : node.children) {
            if (child.role == Document::Role::Animation && child.name == "title") return child.path;
        }
    }
    return {};
}

std::optional<Document::ClipSummary>
ExportedSprite(const std::vector<Document::ClipSummary>& clips) {
    const auto found = std::ranges::find_if(clips, [](const Document::ClipSummary& clip) {
        return clip.id.sprite.has_value() && !clip.name.empty() && clip.frame_count > 1 &&
               clip.frame_count != kTitleFrames;
    });
    if (found == clips.end()) return std::nullopt;
    return *found;
}

std::string Upper(std::string text) {
    std::ranges::transform(text, text.begin(), [](char c) {
        return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
    });
    return text;
}

}

TEST_CASE("An exported sprite shows on its own and afp-core reports its frames") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");

    const std::vector<uint8_t> bytes = ReadAll(dir + "/data/graphic/1/title.ifs");
    REQUIRE(!bytes.empty());
    auto file = Document::File::Open(bytes);
    REQUIRE(file.has_value());
    const std::string path = AnimationPath(*file);
    REQUIRE(!path.empty());
    const auto animation = file->ReadAnimation(path);
    REQUIRE(animation.has_value());
    const std::optional<Document::ClipSummary> sprite = ExportedSprite(Document::Clips(*animation));
    REQUIRE(sprite.has_value());
    if (!sprite) return;
    const std::optional<Document::AnimationDetails> details =
        Document::DescribeClip(*animation, sprite->id);
    REQUIRE(details.has_value());
    if (!details) return;
    INFO(sprite->name + " has " + std::to_string(sprite->frame_count) + " frames");

    auto host =
        PreviewClient::Host::Start(PreviewClient::Options{.host_exe = R573_PREVIEW_HOST_EXE});
    REQUIRE(host.has_value());
    REQUIRE((*host)->Boot(dir, "iidx33").has_value());
    const auto root = (*host)->LoadPackage("title", "title", bytes, false);
    REQUIRE(root.has_value());
    CHECK(root->frame_count == kTitleFrames);

    const auto shown = (*host)->ShowSymbol(sprite->name);
    const std::string show_error = shown.has_value() ? std::string() : shown.error();
    INFO(show_error);
    REQUIRE(shown.has_value());
    CHECK(shown->frame_count == sprite->frame_count);
    CHECK(shown->labels.size() == details->labels.size());

    const uint32_t middle = sprite->frame_count / 2;
    REQUIRE((*host)->Seek(middle).has_value());
    const auto frame = (*host)->Render();
    REQUIRE(frame.has_value());
    CHECK(frame->frame == middle);

    const auto back = (*host)->SelectAnimation("title");
    REQUIRE(back.has_value());
    CHECK(back->frame_count == kTitleFrames);

    const auto folded = (*host)->ShowSymbol(Upper(sprite->name));
    const std::string folded_error = folded.has_value() ? std::string() : folded.error();
    INFO(folded_error);
    REQUIRE(folded.has_value());
    CHECK(folded->frame_count == sprite->frame_count);

    CHECK_FALSE((*host)->ShowSymbol("no symbol is called this").has_value());
}

TEST_CASE("An unexported sprite shows on its own through a preview-only export name") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");

    const std::vector<uint8_t> bytes = ReadAll(dir + "/data/graphic/1/title.ifs");
    REQUIRE(!bytes.empty());
    auto file = Document::File::Open(bytes);
    REQUIRE(file.has_value());
    const std::string path = AnimationPath(*file);
    REQUIRE(!path.empty());
    const auto animation = file->ReadAnimation(path);
    REQUIRE(animation.has_value());
    const std::vector<Document::ClipSummary> clips = Document::Clips(*animation);
    const auto unnamed = std::ranges::find_if(clips, [](const Document::ClipSummary& clip) {
        return clip.id.sprite.has_value() && clip.name.empty() && clip.frame_count != kTitleFrames;
    });
    REQUIRE(unnamed != clips.end());
    INFO("sprite " + std::to_string(unnamed->id.sprite.value_or(0)) + " has " +
         std::to_string(unnamed->frame_count) + " frames");

    const auto preview = Document::PreviewSymbolFor(*file, path, unnamed->id);
    const std::string preview_error = preview.has_value() ? std::string() : preview.error();
    INFO(preview_error);
    REQUIRE(preview.has_value());

    auto host =
        PreviewClient::Host::Start(PreviewClient::Options{.host_exe = R573_PREVIEW_HOST_EXE});
    REQUIRE(host.has_value());
    REQUIRE((*host)->Boot(dir, "iidx33").has_value());
    REQUIRE((*host)->LoadPackage("title", "title", bytes, false).has_value());
    CHECK_FALSE((*host)->ShowSymbol(preview->name).has_value());

    const auto reloaded = (*host)->LoadPackage("title", "title", preview->ifs, true);
    REQUIRE(reloaded.has_value());
    CHECK(reloaded->frame_count == kTitleFrames);
    const auto shown = (*host)->ShowSymbol(preview->name);
    const std::string show_error = shown.has_value() ? std::string() : shown.error();
    INFO(show_error);
    REQUIRE(shown.has_value());
    CHECK(shown->frame_count == unnamed->frame_count);
}
