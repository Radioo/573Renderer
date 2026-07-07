#pragma once

#include "media/media_format.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace VideoEncoder {

using Format = MediaSink::Format;

struct Params {
    std::string output_path;
    Format format = Format::AVIF;

    int src_width = 0;
    int src_height = 0;

    int out_width = 0;
    int out_height = 0;

    int fps = 60;
    int quality = 60;
    int keyframe_interval = 0;

    bool prefer_hardware = true;
};

class Encoder {
public:
    Encoder();
    ~Encoder();
    Encoder(const Encoder&) = delete;
    Encoder& operator=(const Encoder&) = delete;
    Encoder(Encoder&&) noexcept;
    Encoder& operator=(Encoder&&) noexcept;

    bool Create(const Params& raw);
    bool SubmitFrame(const uint8_t* bgra, int frame_index);
    bool Finish();
    void Cancel();

    const std::string& LastError() const { return err_; }

    bool UsingHardware() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string err_;
};

bool HardwareAvailable(Format f = Format::AVIF);

const char* DefaultExtension(Format f);

}
