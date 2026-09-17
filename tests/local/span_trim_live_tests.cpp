#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/document.h"
#include "document/frame_edit.h"
#include "document/outline.h"
#include "document/span_trim.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "preview/preview_client.h"
#include "preview/shared_texture.h"
#include "support/env.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

constexpr uint32_t kViewWidth = 1920;
constexpr uint32_t kViewHeight = 1080;
constexpr uint32_t kThreeD = 0x04000000;
constexpr uint32_t kLaterBy = 10;
constexpr uint32_t kShortestSpan = 30;
const std::string kPackage = "arena";
const std::string kAnimation = "x_panel_broken";

std::vector<uint8_t> ReadAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::string AnimationPath(const Document::File& file) {
    for (const Document::Node& node : file.Nodes()) {
        for (const Document::Node& child : node.children) {
            if (child.role == Document::Role::Animation && child.name == kAnimation)
                return child.path;
        }
    }
    return {};
}

bool IsThreeD(const AfpAnimation::Animation& animation, uint16_t depth) {
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement != nullptr && placement->depth == depth && (placement->flags & kThreeD) != 0)
            return true;
    }
    return false;
}

struct Picked {
    uint16_t depth = 0;
    Document::Span span;
    AfpAnimation::Animation trimmed;
};

std::optional<Picked> TrimmableSpan(const AfpAnimation::Animation& animation) {
    for (const Document::DepthRow& row : Document::DepthRows(animation.root)) {
        if (!IsThreeD(animation, row.depth)) continue;
        for (const Document::Span& span : row.spans) {
            if (span.last_frame - span.first_frame < kShortestSpan) continue;
            AfpAnimation::Animation trimmed = animation;
            const Document::Span wanted{.first_frame = span.first_frame + kLaterBy,
                                        .last_frame = span.last_frame};
            if (!Document::TrimSpan(trimmed, {}, row.depth, span.first_frame, wanted)) continue;
            return Picked{.depth = row.depth, .span = span, .trimmed = std::move(trimmed)};
        }
    }
    return std::nullopt;
}

class Stage {
public:
    explicit Stage(const std::string& dir) {
        auto started =
            PreviewClient::Host::Start(PreviewClient::Options{.host_exe = R573_PREVIEW_HOST_EXE});
        REQUIRE(started.has_value());
        if (!started) return;
        host_ = std::move(*started);
        REQUIRE(host_->Boot(dir, "iidx33").has_value());
        REQUIRE(host_->Resize(kViewWidth, kViewHeight).has_value());
    }

    std::vector<uint8_t> Render(const Document::File& file, uint32_t frame) {
        const auto bytes = file.Encode();
        REQUIRE(bytes.has_value());
        const auto loaded = host_->LoadPackage(kPackage, kAnimation, *bytes, loaded_once_);
        const std::string error = loaded.has_value() ? std::string() : loaded.error();
        INFO(error);
        REQUIRE(loaded.has_value());
        loaded_once_ = true;
        REQUIRE(host_->Seek(frame).has_value());
        const auto shown = host_->Render();
        REQUIRE(shown.has_value());
        auto reader = SharedTexture::Reader::Create();
        REQUIRE(reader.has_value());
        auto pixels = reader->Read(shown->shared_handle, shown->width, shown->height);
        REQUIRE(pixels.has_value());
        return std::move(*pixels);
    }

private:
    std::unique_ptr<PreviewClient::Host> host_;
    bool loaded_once_ = false;
};

}

TEST_CASE("A trimmed 3D span draws its kept frames exactly as before") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");
    auto original = Document::File::Open(ReadAll(dir + "/data/graphic/1/" + kPackage + ".ifs"));
    REQUIRE(original.has_value());
    if (!original) return;
    const std::string path = AnimationPath(*original);
    REQUIRE(!path.empty());
    const auto animation = original->ReadAnimation(path);
    REQUIRE(animation.has_value());
    if (!animation) return;
    const std::optional<Picked> picked = TrimmableSpan(*animation);
    REQUIRE(picked.has_value());
    if (!picked) return;
    INFO("depth " << picked->depth << " frames " << picked->span.first_frame << " to "
                  << picked->span.last_frame);

    Document::File trimmed = *original;
    REQUIRE(trimmed.WriteAnimation(path, picked->trimmed).has_value());
    Document::File without = *original;
    AfpAnimation::Animation removed = *animation;
    REQUIRE(
        Document::RemoveDepth(removed, {}, picked->depth, picked->span.first_frame).has_value());
    REQUIRE(without.WriteAnimation(path, removed).has_value());

    Stage stage(dir);
    const std::array<uint32_t, 3> kept{picked->span.first_frame + kLaterBy,
                                       picked->span.first_frame + kLaterBy + 5,
                                       picked->span.last_frame};
    bool depth_drawn = false;
    for (const uint32_t frame : kept) {
        INFO("frame " << frame);
        const std::vector<uint8_t> before = stage.Render(*original, frame);
        const std::vector<uint8_t> after = stage.Render(trimmed, frame);
        CHECK(after == before);
        depth_drawn = depth_drawn || stage.Render(without, frame) != before;
    }
    CHECK(depth_drawn);
}
