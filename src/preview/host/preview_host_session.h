#pragma once

#include "shared_frame.h"

#include <windows.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
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
    bool LoadContent(const std::vector<uint8_t>& ifs, const std::string& package,
                     const std::string& animation);
    bool LoadTextures(const std::vector<uint8_t>& ifs, const std::string& package);
    std::vector<uint8_t> Resize(const PreviewProtocol::Resize& resize);
    std::vector<uint8_t> Render();
    std::vector<uint8_t> Loaded(std::vector<uint8_t> reply) const;

    HWND window_ = nullptr;
    bool booted_ = false;
    void ApplyInputs() const;

    bool package_loaded_ = false;
    bool background_drawn_ = false;
    struct HeldInput {
        std::string path;
        std::string texture;
        bool hidden = false;
    };

    std::vector<HeldInput> inputs_;
    std::vector<uint8_t> texture_bytes_;
    std::optional<uint32_t> resume_frame_;
    std::optional<SharedFrame::Target> frame_;
    uint32_t view_width_ = 0;
    uint32_t view_height_ = 0;
};

}
