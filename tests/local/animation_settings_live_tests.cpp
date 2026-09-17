#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/animation_settings.h"
#include "document/document.h"
#include "document/outline.h"
#include "document/place_image.h"
#include "preview/preview_client.h"
#include "preview/shared_texture.h"
#include "support/env.h"

#include <algorithm>
#include <array>
#include <cstddef>
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
#include <vector>

namespace {

constexpr uint32_t kViewWidth = 1920;
constexpr uint32_t kViewHeight = 1080;
constexpr std::size_t kBgraBytes = 4;
constexpr uint32_t kFrames = 10;
const std::string kScene = "editor_settings_scene";

std::vector<uint8_t> ReadAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::string TitlePath(const Document::File& file) {
    for (const Document::Node& node : file.Nodes()) {
        for (const Document::Node& child : node.children) {
            if (child.role == Document::Role::Animation && child.name == "title") return child.path;
        }
    }
    return {};
}

std::string SmallestImage(const Document::File& file) {
    std::string best;
    uint32_t best_area = 0;
    for (const Document::Node& node : file.Nodes()) {
        for (const Document::Node& child : node.children) {
            if (child.role != Document::Role::Texture) continue;
            const auto details = file.Describe(child.path);
            if (!details || !details->texture) continue;
            const uint32_t area = details->texture->width * details->texture->height;
            if (area < 64 * 64 || (!best.empty() && area >= best_area)) continue;
            best = details->name;
            best_area = area;
        }
    }
    return best;
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

    std::vector<uint8_t> Render(const Document::File& file) {
        const auto bytes = file.Encode();
        REQUIRE(bytes.has_value());
        const auto loaded = host_->LoadPackage("title", kScene, *bytes, loaded_once_);
        const std::string error = loaded.has_value() ? std::string() : loaded.error();
        INFO(error);
        REQUIRE(loaded.has_value());
        loaded_once_ = true;
        REQUIRE(host_->Seek(1).has_value());
        const auto frame = host_->Render();
        REQUIRE(frame.has_value());
        auto reader = SharedTexture::Reader::Create();
        REQUIRE(reader.has_value());
        auto pixels = reader->Read(frame->shared_handle, frame->width, frame->height);
        REQUIRE(pixels.has_value());
        REQUIRE(pixels->size() == static_cast<std::size_t>(kViewWidth) * kViewHeight * kBgraBytes);
        return std::move(*pixels);
    }

    void DrawBackground(bool drawn) { REQUIRE(host_->SetBackgroundDrawn(drawn).has_value()); }

private:
    std::unique_ptr<PreviewClient::Host> host_;
    bool loaded_once_ = false;
};

std::size_t CountColour(const std::vector<uint8_t>& pixels, std::span<const uint8_t, 4> bgra) {
    std::size_t count = 0;
    for (std::size_t at = 0; at + kBgraBytes <= pixels.size(); at += kBgraBytes) {
        if (std::equal(bgra.begin(), bgra.end(), pixels.begin() + static_cast<std::ptrdiff_t>(at)))
            count++;
    }
    return count;
}

struct Area {
    uint32_t left = kViewWidth;
    uint32_t right = 0;
    uint32_t top = kViewHeight;
    uint32_t bottom = 0;
};

std::optional<Area> ChangedArea(const std::vector<uint8_t>& before,
                                const std::vector<uint8_t>& after) {
    std::optional<Area> area;
    for (uint32_t y = 0; y < kViewHeight; y++) {
        for (uint32_t x = 0; x < kViewWidth; x++) {
            const auto at = static_cast<std::ptrdiff_t>(
                (static_cast<std::size_t>(y) * kViewWidth + x) * kBgraBytes);
            if (std::equal(before.begin() + at, before.begin() + at + kBgraBytes,
                           after.begin() + at))
                continue;
            if (!area) area = Area{};
            area->left = std::min(area->left, x);
            area->right = std::max(area->right, x + 1);
            area->top = std::min(area->top, y);
            area->bottom = std::max(area->bottom, y + 1);
        }
    }
    return area;
}

void Set(Document::File& file, const std::string& path, const std::string& name,
         const std::string& value) {
    auto animation = file.ReadAnimation(path);
    REQUIRE(animation.has_value());
    const auto set = Document::SetAnimationSetting(*animation, name, value);
    REQUIRE(set.has_value());
    REQUIRE(file.WriteAnimation(path, *animation).has_value());
}

std::string Describe(const std::optional<Area>& area) {
    if (!area) return "nothing";
    return std::to_string(area->left) + ".." + std::to_string(area->right) + " x " +
           std::to_string(area->top) + ".." + std::to_string(area->bottom);
}

std::optional<Area> AreaOf(const std::vector<uint8_t>& pixels, std::span<const uint8_t, 4> bgra) {
    const std::vector<uint8_t> none(pixels.size(), 1);
    std::vector<uint8_t> only(pixels.size(), 1);
    for (std::size_t at = 0; at + kBgraBytes <= pixels.size(); at += kBgraBytes) {
        if (std::equal(bgra.begin(), bgra.end(), pixels.begin() + static_cast<std::ptrdiff_t>(at)))
            only[at] = 0;
    }
    return ChangedArea(none, only);
}

}

TEST_CASE("A drawn background fills the stage size in the header's colour") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");
    auto file = Document::File::Open(ReadAll(dir + "/data/graphic/1/title.ifs"));
    REQUIRE(file.has_value());
    if (!file) return;
    const auto path = file->AddAnimation(kScene, TitlePath(*file), kFrames);
    REQUIRE(path.has_value());
    if (!path) return;
    const std::string image = SmallestImage(*file);
    REQUIRE(!image.empty());
    const auto placed = Document::PlaceImage(
        *file, *path, image,
        Document::DepthSpan{.clip = {}, .depth = 1, .first_frame = 0, .last_frame = kFrames - 1});
    REQUIRE(placed.has_value());
    Set(*file, *path, "Background colour", "255, 0, 0, 255");
    Stage stage(dir);
    const std::array<uint8_t, 4> red{0, 0, 255, 255};
    const std::array<uint8_t, 4> black{0, 0, 0, 255};

    const std::vector<uint8_t> hidden = stage.Render(*file);
    CHECK(CountColour(hidden, red) == 0);
    CHECK(CountColour(hidden, black) == 0);

    stage.DrawBackground(true);
    const std::vector<uint8_t> full = stage.Render(*file);
    CHECK(Describe(AreaOf(full, red)) == "0..1920 x 0..1080");
    CHECK(Describe(ChangedArea(hidden, full)) == "0..1920 x 0..1080");

    Set(*file, *path, "Stage size", "960, 540");
    const std::vector<uint8_t> quarter = stage.Render(*file);
    CHECK(Describe(AreaOf(quarter, red)) == "0..960 x 0..540");
    CHECK(Describe(ChangedArea(hidden, quarter)) == "0..960 x 0..540");

    Set(*file, *path, "Use background colour", "off");
    const std::vector<uint8_t> unused = stage.Render(*file);
    CHECK(CountColour(unused, red) == 0);
    CHECK(Describe(AreaOf(unused, black)) == "0..960 x 0..540");
}
