#pragma once

#include "editor_host.h"

#include "document/clip.h"
#include "document/document.h"
#include "document/hidden_depths.h"

#include <QString>

#include <cstdint>
#include <string>
#include <vector>

namespace Editor {

struct HostRequest {
    bool fresh = false;
    Document::File document;
    std::vector<Document::DepthInClip> hidden;
    std::string animation_path;
    std::string package_name;
    std::string animation_name;
    Document::ClipId clip;
    bool wants_in_place = false;
    uint32_t root_frame = 0;
};

struct HostLoad {
    QString refusal;
    uint32_t frame_count = 0;
    bool symbol_shown = false;
    bool loaded = false;
};

[[nodiscard]] HostLoad LoadIntoHost(Host& host, const HostRequest& asked);

}
