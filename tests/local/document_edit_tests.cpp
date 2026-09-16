#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/document.h"
#include "document/history.h"
#include "document/outline.h"
#include "document/placement_edit.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "preview/preview_client.h"
#include "support/env.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

constexpr uint32_t kTitleFrames = 840;
constexpr uint32_t kLoopFrame = 240;
constexpr uint32_t kEditedFrame = 100;
constexpr uint32_t kSeekFrame = 300;

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

}

TEST_CASE("An edited package reloads and afp-core reports the edited animation") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");

    const std::vector<uint8_t> bytes = ReadAll(dir + "/data/graphic/1/title.ifs");
    REQUIRE(!bytes.empty());
    auto file = Document::File::Open(bytes);
    const std::string open_error = file.has_value() ? std::string() : file.error();
    INFO(open_error);
    REQUIRE(file.has_value());
    const std::string path = AnimationPath(*file);
    REQUIRE(!path.empty());

    auto host =
        PreviewClient::Host::Start(PreviewClient::Options{.host_exe = R573_PREVIEW_HOST_EXE});
    REQUIRE(host.has_value());
    REQUIRE((*host)->Boot(dir, "iidx33").has_value());

    const auto first = (*host)->LoadPackage("title", "title", bytes, false);
    const std::string load_error = first.has_value() ? std::string() : first.error();
    INFO(load_error);
    REQUIRE(first.has_value());
    CHECK(first->frame_count == kTitleFrames);
    REQUIRE(first->labels.size() == 1);

    auto animation = file->ReadAnimation(path);
    const std::string read_error = animation.has_value() ? std::string() : animation.error();
    INFO(read_error);
    REQUIRE(animation.has_value());

    const std::vector<Document::DepthRow> rows = Document::DepthRows(animation->root);
    REQUIRE(!rows.empty());
    const auto covering = std::ranges::find_if(rows, [](const Document::DepthRow& row) {
        return std::ranges::any_of(row.spans, [](const Document::Span& span) {
            return span.first_frame <= kSeekFrame && kSeekFrame <= span.last_frame;
        });
    });
    REQUIRE(covering != rows.end());
    const auto live = Document::LivePlacementTag(animation->root, covering->depth, kSeekFrame);
    REQUIRE(live.has_value());
    auto* placement = std::get_if<AfpAnimation::Placement>(&animation->root.tags[*live].body);
    REQUIRE(placement != nullptr);
    const auto moved =
        Document::SetPlacementField(*animation, *placement, "Translation", "1000000, 1000000");
    const std::string set_error = moved.has_value() ? std::string() : moved.error();
    INFO(set_error);
    REQUIRE(moved.has_value());

    animation->strings.emplace_back("edited");
    animation->root.labels.push_back(
        AfpAnimation::Label{.frame = static_cast<uint16_t>(kEditedFrame),
                            .name = static_cast<uint32_t>(animation->strings.size() - 1)});
    std::ranges::sort(animation->root.labels, {}, &AfpAnimation::Label::frame);

    Document::History history;
    CHECK(history.Saved());
    history.Record("move and label", *file);
    const auto written = file->WriteAnimation(path, *animation);
    const std::string write_error = written.has_value() ? std::string() : written.error();
    INFO(write_error);
    REQUIRE(written.has_value());

    const auto encoded = file->Encode();
    const std::string encode_error = encoded.has_value() ? std::string() : encoded.error();
    INFO(encode_error);
    REQUIRE(encoded.has_value());

    const auto reloaded = (*host)->LoadPackage("title", "title", *encoded, true);
    const std::string reload_error = reloaded.has_value() ? std::string() : reloaded.error();
    INFO(reload_error);
    REQUIRE(reloaded.has_value());
    CHECK(reloaded->frame_count == kTitleFrames);
    REQUIRE(reloaded->labels.size() == 2);
    CHECK(reloaded->labels[0].name == "edited");
    CHECK(reloaded->labels[0].frame == kEditedFrame);
    CHECK(reloaded->labels[1].name == "loop");
    CHECK(reloaded->labels[1].frame == kLoopFrame);

    REQUIRE((*host)->Seek(kSeekFrame).has_value());
    const auto frame = (*host)->Render();
    REQUIRE(frame.has_value());
    CHECK(frame->frame == kSeekFrame);

    CHECK_FALSE(history.Saved());
    auto undone = history.Undo(*file);
    REQUIRE(undone.has_value());
    CHECK(history.Saved());
    const Document::File restored = std::move(undone).value_or(Document::File{});
    const auto undone_bytes = restored.Encode();
    REQUIRE(undone_bytes.has_value());
    const auto back = (*host)->LoadPackage("title", "title", *undone_bytes, true);
    const std::string back_error = back.has_value() ? std::string() : back.error();
    INFO(back_error);
    REQUIRE(back.has_value());
    CHECK(back->frame_count == kTitleFrames);
    REQUIRE(back->labels.size() == 1);
    CHECK(back->labels[0].name == "loop");
    CHECK(back->labels[0].frame == kLoopFrame);
}
