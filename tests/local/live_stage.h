#pragma once

#include "document/document.h"
#include "preview/preview_client.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace LiveStage {

struct Target {
    std::string package;
    std::string animation;
};

[[nodiscard]] std::vector<uint8_t> ReadAll(const std::filesystem::path& path);

[[nodiscard]] std::string AnimationPath(const Document::File& file, const std::string& name);

class Stage {
public:
    Stage(const std::string& dir, Target target);

    std::vector<uint8_t> Render(const Document::File& file, uint32_t frame);

private:
    Target target_;
    std::unique_ptr<PreviewClient::Host> host_;
    bool loaded_once_ = false;
};

}
