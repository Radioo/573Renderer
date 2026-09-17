#include "editor_host.h"

#include "preview/preview_client.h"
#include "support/expected.h"

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <utility>

namespace Editor {

namespace {

constexpr const char* kHostExeName = "preview_host.exe";

}

std::string FindPreviewHost(std::span<const std::string> search_dirs) {
    std::error_code ec;
    for (const std::string& dir : search_dirs) {
        const std::filesystem::path candidate = std::filesystem::path(dir) / kHostExeName;
        if (std::filesystem::exists(candidate, ec)) return candidate.string();
    }
    return {};
}

Support::Expected<void, std::string>
Host::Start(const std::string& host_exe, const std::string& game_dir, const std::string& build) {
    Stop();
    auto host = PreviewClient::Host::Start(PreviewClient::Options{.host_exe = host_exe});
    if (!host) return Support::Unexpected(host.error());
    auto booted = (*host)->Boot(game_dir, build);
    if (!booted) return Support::Unexpected(booted.error());
    host_ = std::move(*host);
    return {};
}

void Host::Stop() {
    host_.reset();
    package_.clear();
}

Support::Expected<PreviewClient::Loaded, std::string>
Host::ShowAnimation(const std::string& package, const std::string& animation,
                    std::span<const uint8_t> ifs) {
    if (host_ == nullptr) return Support::Unexpected(std::string("no preview host is running"));
    if (package == package_) return host_->SelectAnimation(animation);
    auto loaded = host_->LoadPackage(package, animation, ifs, !package_.empty());
    if (!loaded) return Support::Unexpected(loaded.error());
    package_ = package;
    return loaded;
}

Support::Expected<PreviewClient::Loaded, std::string> Host::Reload(const std::string& package,
                                                                   const std::string& animation,
                                                                   std::span<const uint8_t> ifs) {
    if (host_ == nullptr) return Support::Unexpected(std::string("no preview host is running"));
    auto loaded = host_->LoadPackage(package, animation, ifs, !package_.empty());
    if (!loaded) return Support::Unexpected(loaded.error());
    package_ = package;
    return loaded;
}

Support::Expected<PreviewClient::Loaded, std::string> Host::ShowSymbol(const std::string& name) {
    if (host_ == nullptr) return Support::Unexpected(std::string("no preview host is running"));
    return host_->ShowSymbol(name);
}

Support::Expected<void, std::string> Host::Seek(uint32_t frame) {
    if (host_ == nullptr) return Support::Unexpected(std::string("no preview host is running"));
    return host_->Seek(frame);
}

Support::Expected<void, std::string> Host::Resize(uint32_t width, uint32_t height) {
    if (host_ == nullptr) return Support::Unexpected(std::string("no preview host is running"));
    return host_->Resize(width, height);
}

Support::Expected<void, std::string> Host::SetBackgroundDrawn(bool drawn) {
    if (host_ == nullptr) return Support::Unexpected(std::string("no preview host is running"));
    return host_->SetBackgroundDrawn(drawn);
}

Support::Expected<PreviewClient::Frame, std::string> Host::Render() {
    if (host_ == nullptr) return Support::Unexpected(std::string("no preview host is running"));
    return host_->Render();
}

}
