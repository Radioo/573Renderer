#pragma once

#include "preview/preview_channel.h"
#include "support/expected.h"

#include <windows.h>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace PreviewClient {

struct Options {
    std::string host_exe;
    unsigned connect_timeout_ms = 20000;
    unsigned reply_timeout_ms = 120000;
};

struct Label {
    std::string name;
    uint32_t frame = 0;
};

struct InputValue {
    std::string path;
    std::string texture;
    bool hidden = false;
};

struct Loaded {
    uint32_t frame_count = 0;
    std::vector<Label> labels;
};

struct Frame {
    uint64_t shared_handle = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t frame = 0;
    uint32_t stage_width = 0;
    uint32_t stage_height = 0;
};

class Host {
public:
    Host(PreviewChannel::Client client, HANDLE process);
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;
    Host(Host&&) = delete;
    Host& operator=(Host&&) = delete;
    ~Host();

    [[nodiscard]] static Support::Expected<std::unique_ptr<Host>, std::string>
    Start(const Options& options);

    [[nodiscard]] Support::Expected<void, std::string> Boot(const std::string& game_dir,
                                                            const std::string& build);

    [[nodiscard]] Support::Expected<Loaded, std::string> LoadPackage(const std::string& package,
                                                                     const std::string& animation,
                                                                     std::span<const uint8_t> ifs,
                                                                     bool reload);

    [[nodiscard]] Support::Expected<Loaded, std::string> SelectAnimation(const std::string& name);

    [[nodiscard]] Support::Expected<Loaded, std::string> ShowSymbol(const std::string& name);
    [[nodiscard]] Support::Expected<void, std::string>
    SetInputs(const std::vector<InputValue>& values);

    [[nodiscard]] Support::Expected<void, std::string> Seek(uint32_t frame);

    [[nodiscard]] Support::Expected<void, std::string> Resize(uint32_t width, uint32_t height);

    [[nodiscard]] Support::Expected<Frame, std::string> Render();

    [[nodiscard]] Support::Expected<void, std::string> SetBackgroundDrawn(bool drawn);

    [[nodiscard]] const std::string& LastRequest() const { return client_.LastRequest(); }

private:
    [[nodiscard]] Support::Expected<std::vector<uint8_t>, std::string>
    Call(std::span<const uint8_t> request, const std::string& name);

    PreviewChannel::Client client_;
    HANDLE process_ = nullptr;
};

}
