#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "media/media_format.h"

namespace MediaSink {

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

class Sink {
public:
    Sink();
    ~Sink();
    Sink(const Sink&) = delete;
    Sink& operator=(const Sink&) = delete;
    Sink(Sink&&) noexcept;
    Sink& operator=(Sink&&) noexcept;

    bool Open(const Params& p);

    bool SubmitFrame(const uint8_t* bgra, int frame_index);

    bool Finish();

    void Cancel();

    bool UsingHardware() const;

    Format ActiveFormat() const;
    const std::string& OutputPath() const;
    const std::string& LastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
