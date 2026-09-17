#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/document.h"
#include "document/outline.h"
#include "document/place_image.h"
#include "document/timeline.h"
#include "preview/preview_client.h"
#include "preview/shared_texture.h"
#include "support/env.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr uint32_t kViewWidth = 1920;
constexpr uint32_t kViewHeight = 1080;
constexpr uint32_t kShownFrame = 10;
constexpr uint32_t kLastFrame = 20;
constexpr std::size_t kBgraBytes = 4;

struct Picked {
    std::string name;
    uint32_t width = 0;
    uint32_t height = 0;
};

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

std::optional<Picked> LargestImage(const Document::File& file) {
    std::optional<Picked> best;
    for (const Document::Node& node : file.Nodes()) {
        for (const Document::Node& child : node.children) {
            if (child.role != Document::Role::Texture) continue;
            const auto details = file.Describe(child.path);
            if (!details || !details->texture) continue;
            const uint32_t area = details->texture->width * details->texture->height;
            if (best && area <= best->width * best->height) continue;
            best = Picked{.name = details->name,
                          .width = details->texture->width,
                          .height = details->texture->height};
        }
    }
    return best;
}

uint16_t FreeDepth(const Document::File& file, const std::string& path) {
    const auto details = file.Describe(path);
    uint16_t highest = 0;
    if (details && details->animation) {
        for (const Document::DepthRow& row : details->animation->depths)
            highest = std::max(highest, row.depth);
    }
    return static_cast<uint16_t>(highest + 1);
}

std::vector<uint8_t> RenderPixels(PreviewClient::Host& host, std::span<const uint8_t> ifs,
                                  bool reload) {
    REQUIRE(host.LoadPackage("title", "title", ifs, reload).has_value());
    REQUIRE(host.Seek(kShownFrame).has_value());
    const auto frame = host.Render();
    REQUIRE(frame.has_value());
    auto reader = SharedTexture::Reader::Create();
    REQUIRE(reader.has_value());
    auto pixels = reader->Read(frame->shared_handle, frame->width, frame->height);
    REQUIRE(pixels.has_value());
    return std::move(*pixels);
}

struct Changed {
    std::size_t inside = 0;
    std::size_t outside = 0;
};

Changed Compare(const std::vector<uint8_t>& before, const std::vector<uint8_t>& after,
                const Picked& image) {
    Changed changed;
    for (uint32_t y = 0; y < kViewHeight; y++) {
        for (uint32_t x = 0; x < kViewWidth; x++) {
            const std::size_t at = (static_cast<std::size_t>(y) * kViewWidth + x) * kBgraBytes;
            const auto first = static_cast<std::ptrdiff_t>(at);
            const auto last = first + static_cast<std::ptrdiff_t>(kBgraBytes);
            if (std::equal(before.begin() + first, before.begin() + last, after.begin() + first))
                continue;
            if (x < image.width && y < image.height) {
                changed.inside++;
            } else {
                changed.outside++;
            }
        }
    }
    return changed;
}

}

TEST_CASE("A package image placed as a new shape draws where the quad is") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");

    const std::vector<uint8_t> bytes = ReadAll(dir + "/data/graphic/1/title.ifs");
    REQUIRE(!bytes.empty());
    auto file = Document::File::Open(bytes);
    REQUIRE(file.has_value());
    const std::string path = AnimationPath(*file);
    REQUIRE(!path.empty());
    const std::optional<Picked> image = LargestImage(*file);
    REQUIRE(image.has_value());
    if (!image) return;
    INFO(image->name + " is " + std::to_string(image->width) + "x" + std::to_string(image->height));

    const auto placed = Document::PlaceImage(*file, path, image->name,
                                             Document::DepthSpan{.clip = {},
                                                                 .depth = FreeDepth(*file, path),
                                                                 .first_frame = 0,
                                                                 .last_frame = kLastFrame});
    const std::string place_error = placed.has_value() ? std::string() : placed.error();
    INFO(place_error);
    REQUIRE(placed.has_value());
    if (!placed) return;
    const std::map<uint16_t, std::string> shapes = file->ShapeImages(path);
    CHECK(shapes.size() > 1);
    const auto named = shapes.find(*placed);
    REQUIRE(named != shapes.end());
    CHECK(named->second == image->name);
    const auto edited = file->Encode();
    REQUIRE(edited.has_value());

    auto host =
        PreviewClient::Host::Start(PreviewClient::Options{.host_exe = R573_PREVIEW_HOST_EXE});
    REQUIRE(host.has_value());
    REQUIRE((*host)->Boot(dir, "iidx33").has_value());
    REQUIRE((*host)->Resize(kViewWidth, kViewHeight).has_value());
    const std::vector<uint8_t> before = RenderPixels(**host, bytes, false);
    const std::vector<uint8_t> after = RenderPixels(**host, *edited, true);
    REQUIRE(before.size() == static_cast<std::size_t>(kViewWidth) * kViewHeight * kBgraBytes);
    REQUIRE(after.size() == before.size());

    const Changed changed = Compare(before, after, *image);
    INFO(std::to_string(changed.inside) + " pixels changed inside the quad, " +
         std::to_string(changed.outside) + " outside");
    CHECK(changed.inside > 0);
    CHECK(changed.outside == 0);
}
