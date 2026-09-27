#include "live_stage.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/document.h"
#include "document/outline.h"
#include "preview/preview_client.h"
#include "preview/shared_texture.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace LiveStage {

namespace {

constexpr uint32_t kViewWidth = 1920;
constexpr uint32_t kViewHeight = 1080;

}

std::vector<uint8_t> ReadAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::string AnimationPath(const Document::File& file, const std::string& name) {
    for (const Document::Node& node : file.Nodes()) {
        for (const Document::Node& child : node.children) {
            if (child.role == Document::Role::Animation && child.name == name) return child.path;
        }
    }
    return {};
}

Stage::Stage(const std::string& dir, Target target) : target_(std::move(target)) {
    auto started =
        PreviewClient::Host::Start(PreviewClient::Options{.host_exe = R573_PREVIEW_HOST_EXE});
    REQUIRE(started.has_value());
    if (!started) return;
    host_ = std::move(*started);
    REQUIRE(host_->Boot(dir, "iidx33").has_value());
    REQUIRE(host_->Resize(kViewWidth, kViewHeight).has_value());
}

std::vector<uint8_t> Stage::Render(const Document::File& file, uint32_t frame) {
    const auto bytes = file.Encode();
    REQUIRE(bytes.has_value());
    const auto loaded =
        host_->LoadPackage(target_.package, target_.animation, *bytes, loaded_once_);
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

}
