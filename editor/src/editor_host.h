#pragma once

#include "preview/preview_client.h"
#include "support/expected.h"

#include <memory>
#include <span>
#include <string>
#include <vector>

namespace Editor {

[[nodiscard]] std::string FindPreviewHost(std::span<const std::string> search_dirs);

class Host {
public:
    [[nodiscard]] Support::Expected<void, std::string>
    Start(const std::string& host_exe, const std::string& game_dir, const std::string& build);

    void Stop();

    [[nodiscard]] bool Running() const { return host_ != nullptr; }

    [[nodiscard]] Support::Expected<PreviewClient::Loaded, std::string>
    ShowAnimation(const std::string& package, const std::string& animation,
                  std::span<const uint8_t> ifs);

    [[nodiscard]] Support::Expected<PreviewClient::Loaded, std::string>
    Reload(const std::string& package, const std::string& animation, std::span<const uint8_t> ifs);

    [[nodiscard]] Support::Expected<PreviewClient::Loaded, std::string>
    ShowSymbol(const std::string& name);

    [[nodiscard]] Support::Expected<void, std::string> Seek(uint32_t frame);

    [[nodiscard]] Support::Expected<void, std::string>
    SetInputs(const std::vector<PreviewClient::InputValue>& values);

    [[nodiscard]] Support::Expected<void, std::string> Resize(uint32_t width, uint32_t height);

    [[nodiscard]] Support::Expected<PreviewClient::Frame, std::string> Render();

    [[nodiscard]] Support::Expected<void, std::string> SetBackgroundDrawn(bool drawn);

private:
    std::unique_ptr<PreviewClient::Host> host_;
    std::string package_;
};

}
