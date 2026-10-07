#pragma once

#include "encode/frame_section.h"
#include "media/media_format.h"
#include "media_sink.h"
#include "preview/preview_channel.h"
#include "support/expected.h"

#include <windows.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace EncodeClient {

class Host {
public:
    explicit Host(std::wstring host_exe);
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;
    Host(Host&&) = delete;
    Host& operator=(Host&&) = delete;
    ~Host();

    [[nodiscard]] Support::Expected<std::vector<uint8_t>, std::string>
    Call(std::span<const uint8_t> request, const std::string& name);

    [[nodiscard]] uint32_t NextSession() { return next_session_++; }

    [[nodiscard]] bool HardwareAvailable(MediaSink::Format probe_format);

private:
    [[nodiscard]] Support::Expected<void, std::string> EnsureRunning();
    void Stop();

    std::wstring host_exe_;
    std::mutex mutex_;
    std::unique_ptr<PreviewChannel::Client> client_;
    HANDLE process_ = nullptr;
    std::atomic<uint32_t> next_session_{1};
    std::mutex probed_mutex_;
    std::map<MediaSink::Format, bool> probed_;
};

[[nodiscard]] Host& DefaultHost();

class Sink {
public:
    Sink() : host_(&DefaultHost()) {}
    explicit Sink(Host& host) : host_(&host) {}
    ~Sink();
    Sink(const Sink&) = delete;
    Sink& operator=(const Sink&) = delete;
    Sink(Sink&& other) noexcept;
    Sink& operator=(Sink&& other) noexcept;

    bool Open(const MediaSink::Params& p);

    bool SubmitFrame(const uint8_t* bgra, int frame_index);

    bool Finish();

    void Cancel();

    [[nodiscard]] bool UsingHardware() const { return opened_ && using_hardware_; }

    [[nodiscard]] const std::string& LastError() const { return err_; }

private:
    [[nodiscard]] Support::Expected<std::vector<uint8_t>, std::string>
    Ask(std::span<const uint8_t> request, const std::string& name);
    bool Fail(const std::string& error);
    void CancelQuietly() noexcept;

    Host* host_;
    std::unique_ptr<EncodeFrames::Section> frames_;
    std::size_t frame_bytes_ = 0;
    uint32_t session_ = 0;
    bool opened_ = false;
    bool using_hardware_ = false;
    std::string err_;
};

[[nodiscard]] bool HardwareAvailable(MediaSink::Format probe_format);

}
