#pragma once

#include "shared_frame.h"

#include <windows.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace PreviewProtocol {
struct Boot;
struct LoadPackage;
struct SelectAnimation;
struct Seek;
struct Resize;
}

namespace PreviewHost {

class Session {
public:
    Session() = default;
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&) = delete;
    Session& operator=(Session&&) = delete;
    ~Session();

    [[nodiscard]] std::vector<uint8_t> Handle(std::span<const uint8_t> request);

private:
    std::vector<uint8_t> Boot(const PreviewProtocol::Boot& boot);
    std::vector<uint8_t> LoadPackage(const PreviewProtocol::LoadPackage& load);
    std::vector<uint8_t> Resize(const PreviewProtocol::Resize& resize);
    std::vector<uint8_t> Render();
    std::vector<uint8_t> Loaded(std::vector<uint8_t> reply) const;

    HWND window_ = nullptr;
    bool booted_ = false;
    bool package_loaded_ = false;
    bool background_drawn_ = false;
    std::optional<SharedFrame::Target> frame_;
    uint32_t view_width_ = 0;
    uint32_t view_height_ = 0;
};

}
