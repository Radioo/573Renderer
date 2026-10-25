#include "encode/encode_client.h"

#include "encode/frame_section.h"
#include "encode_host_generated.h"
#include "media/media_format.h"
#include "media_sink.h"
#include "preview/preview_channel.h"
#include "support/expected.h"
#include "support/log.h"

#include <flatbuffers/buffer.h>
#include <flatbuffers/flatbuffer_builder.h>
#include <flatbuffers/verifier.h>

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace EncodeClient {

namespace {

constexpr unsigned kConnectTimeoutMs = 20000;
constexpr unsigned kExitWaitMs = 5000;
constexpr std::size_t kBytesPerPixel = 4;
constexpr wchar_t kHostExeName[] = L"573Encoder.exe";

std::vector<uint8_t> Finished(const flatbuffers::FlatBufferBuilder& builder) {
    const std::span<const uint8_t> bytes(builder.GetBufferPointer(), builder.GetSize());
    return {bytes.begin(), bytes.end()};
}

template <typename T>
std::vector<uint8_t> Request(flatbuffers::FlatBufferBuilder& builder, EncodeProtocol::Request type,
                             flatbuffers::Offset<T> body) {
    builder.Finish(EncodeProtocol::CreateRequestMessage(builder, type, body.Union()));
    return Finished(builder);
}

Support::Expected<const EncodeProtocol::ReplyMessage*, std::string>
Decode(const std::vector<uint8_t>& reply, const std::string& request) {
    flatbuffers::Verifier verifier(reply.data(), reply.size());
    if (!verifier.VerifyBuffer<EncodeProtocol::ReplyMessage>(nullptr)) {
        return Support::Unexpected(
            std::format("the encoder's answer to {} is not a reply", request));
    }
    const auto* message = flatbuffers::GetRoot<EncodeProtocol::ReplyMessage>(reply.data());
    if (const auto* failure = message->reply_as_Failure()) {
        return Support::Unexpected(failure->message() != nullptr ? failure->message()->str()
                                                                 : request + " failed");
    }
    return message;
}

std::string Narrow(const std::wstring& wide) {
    const int length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                           nullptr, 0, nullptr, nullptr);
    std::string narrow(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), narrow.data(),
                        length, nullptr, nullptr);
    return narrow;
}

std::string UniqueName(const char* kind, uint32_t session) {
    return std::format(R"({}r573_encode_{}_{})", kind, GetCurrentProcessId(), session);
}

std::wstring BesideThisExe(const wchar_t* name) {
    std::array<wchar_t, MAX_PATH> path{};
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    return (std::filesystem::path(std::wstring(path.data(), length)).parent_path() / name)
        .wstring();
}

}

Host::Host(std::wstring host_exe) : host_exe_(std::move(host_exe)) {}

Host::~Host() {
    const std::scoped_lock lock(mutex_);
    Stop();
}

void Host::Stop() {
    client_.reset();
    if (process_ == nullptr) return;
    if (WaitForSingleObject(process_, kExitWaitMs) != WAIT_OBJECT_0) TerminateProcess(process_, 1);
    CloseHandle(process_);
    process_ = nullptr;
}

Support::Expected<void, std::string> Host::EnsureRunning() {
    if (client_ != nullptr && WaitForSingleObject(process_, 0) == WAIT_TIMEOUT) return {};
    Stop();
    const std::string pipe_name = UniqueName(R"(\\.\pipe\)", next_session_++);
    std::wstring command =
        std::format(LR"("{}" "{}")", host_exe_, std::wstring(pipe_name.begin(), pipe_name.end()));
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    if (CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                       nullptr, &startup, &process) == FALSE) {
        return Support::Unexpected(std::format("cannot start the encoder {} (error {})",
                                               Narrow(host_exe_), GetLastError()));
    }
    CloseHandle(process.hThread);
    process_ = process.hProcess;
    auto pipe = PreviewChannel::Connect(pipe_name, kConnectTimeoutMs);
    if (!pipe) {
        Stop();
        return Support::Unexpected(std::format("the encoder did not answer: {}", pipe.error()));
    }
    client_ = std::make_unique<PreviewChannel::Client>(std::move(*pipe), INFINITE);
    return {};
}

Support::Expected<std::vector<uint8_t>, std::string> Host::Call(std::span<const uint8_t> request,
                                                                const std::string& name) {
    const std::scoped_lock lock(mutex_);
    auto running = EnsureRunning();
    if (!running) return Support::Unexpected(running.error());
    auto reply = client_->Call(request, name);
    if (!reply) Stop();
    return reply;
}

bool Host::HardwareAvailable(MediaSink::Format probe_format) {
    {
        const std::scoped_lock lock(probed_mutex_);
        const auto known = probed_.find(probe_format);
        if (known != probed_.end()) return known->second;
    }
    flatbuffers::FlatBufferBuilder builder;
    const auto request =
        Request(builder, EncodeProtocol::Request::Probe,
                EncodeProtocol::CreateProbe(builder, MediaSink::ToIndex(probe_format)));
    bool available = false;
    auto reply = Call(request, "Probe");
    if (reply) {
        auto decoded = Decode(*reply, "Probe");
        if (decoded && (*decoded)->reply_as_Probed() != nullptr)
            available = (*decoded)->reply_as_Probed()->available();
        if (!decoded) LOG("Encoder", "hardware probe failed: %s", decoded.error().c_str());
    } else {
        LOG("Encoder", "hardware probe failed: %s", reply.error().c_str());
    }
    const std::scoped_lock lock(probed_mutex_);
    probed_[probe_format] = available;
    return available;
}

Host& DefaultHost() {
    static Host host(BesideThisExe(kHostExeName));
    return host;
}

bool HardwareAvailable(MediaSink::Format probe_format) {
    return DefaultHost().HardwareAvailable(probe_format);
}

Sink::~Sink() {
    CancelQuietly();
}

Sink::Sink(Sink&& other) noexcept
    : host_(other.host_), frames_(std::move(other.frames_)), frame_bytes_(other.frame_bytes_),
      session_(other.session_), opened_(std::exchange(other.opened_, false)),
      using_hardware_(other.using_hardware_), err_(std::move(other.err_)) {}

Sink& Sink::operator=(Sink&& other) noexcept {
    if (this == &other) return *this;
    CancelQuietly();
    host_ = other.host_;
    frames_ = std::move(other.frames_);
    frame_bytes_ = other.frame_bytes_;
    session_ = other.session_;
    opened_ = std::exchange(other.opened_, false);
    using_hardware_ = other.using_hardware_;
    err_ = std::move(other.err_);
    return *this;
}

void Sink::CancelQuietly() noexcept {
    try {
        Cancel();
    } catch (...) {
        LOG("Encoder", "cancel threw, ignoring");
    }
}

bool Sink::Fail(const std::string& error) {
    err_ = error;
    return false;
}

Support::Expected<std::vector<uint8_t>, std::string> Sink::Ask(std::span<const uint8_t> request,
                                                               const std::string& name) {
    auto reply = host_->Call(request, name);
    if (!reply) return Support::Unexpected(std::format("the encoder stopped: {}", reply.error()));
    auto decoded = Decode(*reply, name);
    if (!decoded) return Support::Unexpected(decoded.error());
    return reply;
}

bool Sink::Open(const MediaSink::Params& p) {
    if (opened_) return Fail("MediaSink::Open: already opened");
    session_ = host_->NextSession();
    frame_bytes_ = static_cast<std::size_t>(p.src_width) * static_cast<std::size_t>(p.src_height) *
                   kBytesPerPixel;
    const std::string frames_name = UniqueName(R"(Local\)", session_);
    auto frames = EncodeFrames::Section::Create(frames_name, frame_bytes_);
    if (!frames) return Fail(frames.error());
    frames_ = std::move(*frames);

    flatbuffers::FlatBufferBuilder builder;
    const auto request =
        Request(builder, EncodeProtocol::Request::Open,
                EncodeProtocol::CreateOpenDirect(
                    builder, session_, p.output_path.c_str(), MediaSink::ToIndex(p.format),
                    p.src_width, p.src_height, p.out_width, p.out_height, p.fps, p.quality,
                    p.keyframe_interval, p.prefer_hardware, frames_name.c_str()));
    auto reply = Ask(request, "Open");
    if (!reply) {
        frames_.reset();
        return Fail(reply.error());
    }
    const auto* opened =
        flatbuffers::GetRoot<EncodeProtocol::ReplyMessage>(reply->data())->reply_as_Opened();
    using_hardware_ = opened != nullptr && opened->using_hardware();
    opened_ = true;
    return true;
}

bool Sink::SubmitFrame(const uint8_t* bgra, int frame_index) {
    if (!opened_) return Fail("MediaSink::SubmitFrame: not opened");
    std::memcpy(frames_->Data(), bgra, frame_bytes_);
    flatbuffers::FlatBufferBuilder builder;
    const auto request = Request(builder, EncodeProtocol::Request::Frame,
                                 EncodeProtocol::CreateFrame(builder, session_, frame_index));
    auto reply = Ask(request, "Frame");
    if (!reply) return Fail(reply.error());
    return true;
}

bool Sink::Finish() {
    if (!opened_) return false;
    flatbuffers::FlatBufferBuilder builder;
    const auto request = Request(builder, EncodeProtocol::Request::FinishOutput,
                                 EncodeProtocol::CreateFinishOutput(builder, session_));
    auto reply = Ask(request, "Finish");
    opened_ = false;
    frames_.reset();
    if (!reply) return Fail(reply.error());
    return true;
}

void Sink::Cancel() {
    if (!opened_) return;
    flatbuffers::FlatBufferBuilder builder;
    const auto request = Request(builder, EncodeProtocol::Request::Cancel,
                                 EncodeProtocol::CreateCancel(builder, session_));
    auto reply = Ask(request, "Cancel");
    if (!reply) LOG("Encoder", "cancel not confirmed: %s", reply.error().c_str());
    opened_ = false;
    frames_.reset();
}

}
