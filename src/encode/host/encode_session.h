#pragma once

#include "encode/frame_section.h"
#include "media_sink.h"

#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace EncodeHost {

class Session {
public:
    [[nodiscard]] std::vector<uint8_t> Handle(std::span<const uint8_t> request);

private:
    struct Output {
        MediaSink::Sink sink;
        std::unique_ptr<EncodeFrames::Section> frames;
    };

    [[nodiscard]] Output* Find(uint32_t session);

    std::map<uint32_t, std::unique_ptr<Output>> outputs_;
};

}
