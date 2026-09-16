#include "preview/preview_client.h"

#include "preview/preview_channel.h"
#include "preview_host_generated.h"
#include "support/expected.h"

#include <flatbuffers/buffer.h>
#include <flatbuffers/flatbuffer_builder.h>
#include <flatbuffers/verifier.h>

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace PreviewClient {

namespace {

constexpr unsigned kExitWaitMs = 5000;

std::string UniquePipeName() {
    static std::atomic<unsigned> counter{0};
    return std::format(R"(\\.\pipe\r573_preview_{}_{})", GetCurrentProcessId(), counter++);
}

std::vector<uint8_t> Finished(const flatbuffers::FlatBufferBuilder& builder) {
    const std::span<const uint8_t> bytes(builder.GetBufferPointer(), builder.GetSize());
    return {bytes.begin(), bytes.end()};
}

Support::Expected<const PreviewProtocol::ReplyMessage*, std::string>
Decode(const std::vector<uint8_t>& reply, const std::string& request) {
    flatbuffers::Verifier verifier(reply.data(), reply.size());
    if (!verifier.VerifyBuffer<PreviewProtocol::ReplyMessage>(nullptr))
        return Support::Unexpected(std::format("the host's answer to {} is not a reply", request));
    const auto* message = flatbuffers::GetRoot<PreviewProtocol::ReplyMessage>(reply.data());
    if (const auto* failure = message->reply_as_Failure()) {
        const std::string text = failure->message() != nullptr ? failure->message()->str() : "";
        return Support::Unexpected(std::format("{} failed: {}", request, text));
    }
    return message;
}

Loaded ReadLoaded(const PreviewProtocol::Loaded& loaded) {
    Loaded out{.frame_count = loaded.frame_count(), .labels = {}};
    if (loaded.labels() == nullptr) return out;
    for (const auto* label : *loaded.labels()) {
        out.labels.push_back(
            Label{.name = label->name() != nullptr ? label->name()->str() : std::string(),
                  .frame = label->frame()});
    }
    return out;
}

Support::Expected<HANDLE, std::string> StartProcess(const std::string& host_exe,
                                                    const std::string& pipe_name) {
    const std::string narrow = std::format(R"("{}" "{}")", host_exe, pipe_name);
    std::wstring command(narrow.begin(), narrow.end());
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    if (CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                       nullptr, &startup, &process) == FALSE) {
        return Support::Unexpected(
            std::format("cannot start {} (error {})", host_exe, GetLastError()));
    }
    CloseHandle(process.hThread);
    return process.hProcess;
}

}

Host::Host(PreviewChannel::Client client, HANDLE process)
    : client_(std::move(client)), process_(process) {}

Host::~Host() {
    client_.Close();
    if (process_ == nullptr) return;
    if (WaitForSingleObject(process_, kExitWaitMs) != WAIT_OBJECT_0) TerminateProcess(process_, 1);
    CloseHandle(process_);
}

Support::Expected<std::unique_ptr<Host>, std::string> Host::Start(const Options& options) {
    const std::string pipe_name = UniquePipeName();
    auto process = StartProcess(options.host_exe, pipe_name);
    if (!process) return Support::Unexpected(process.error());
    auto pipe = PreviewChannel::Connect(pipe_name, options.connect_timeout_ms);
    if (!pipe) {
        TerminateProcess(*process, 1);
        CloseHandle(*process);
        return Support::Unexpected(
            std::format("the preview host did not answer: {}", pipe.error()));
    }
    return std::make_unique<Host>(
        PreviewChannel::Client(std::move(*pipe), options.reply_timeout_ms), *process);
}

Support::Expected<std::vector<uint8_t>, std::string> Host::Call(std::span<const uint8_t> request,
                                                                const std::string& name) {
    return client_.Call(request, name);
}

Support::Expected<void, std::string> Host::Boot(const std::string& game_dir,
                                                const std::string& build) {
    flatbuffers::FlatBufferBuilder builder;
    const auto boot = PreviewProtocol::CreateBoot(builder, builder.CreateString(game_dir),
                                                  builder.CreateString(build));
    builder.Finish(PreviewProtocol::CreateRequestMessage(builder, PreviewProtocol::Request::Boot,
                                                         boot.Union()));
    auto reply = Call(Finished(builder), "Boot");
    if (!reply) return Support::Unexpected(reply.error());
    auto message = Decode(*reply, "Boot");
    if (!message) return Support::Unexpected(message.error());
    return {};
}

Support::Expected<Loaded, std::string> Host::LoadPackage(const std::string& package,
                                                         const std::string& animation,
                                                         std::span<const uint8_t> ifs,
                                                         bool reload) {
    flatbuffers::FlatBufferBuilder builder;
    const auto package_name = builder.CreateString(package);
    const auto animation_name = builder.CreateString(animation);
    const auto bytes = builder.CreateVector(ifs.data(), ifs.size());
    const auto load =
        PreviewProtocol::CreateLoadPackage(builder, package_name, animation_name, bytes, reload);
    builder.Finish(PreviewProtocol::CreateRequestMessage(
        builder, PreviewProtocol::Request::LoadPackage, load.Union()));
    auto reply = Call(Finished(builder), "LoadPackage");
    if (!reply) return Support::Unexpected(reply.error());
    auto message = Decode(*reply, "LoadPackage");
    if (!message) return Support::Unexpected(message.error());
    const auto* loaded = (*message)->reply_as_Loaded();
    if (loaded == nullptr)
        return Support::Unexpected(std::string("the host did not answer LoadPackage with Loaded"));
    return ReadLoaded(*loaded);
}

Support::Expected<Loaded, std::string> Host::SelectAnimation(const std::string& name) {
    flatbuffers::FlatBufferBuilder builder;
    const auto select = PreviewProtocol::CreateSelectAnimation(builder, builder.CreateString(name));
    builder.Finish(PreviewProtocol::CreateRequestMessage(
        builder, PreviewProtocol::Request::SelectAnimation, select.Union()));
    auto reply = Call(Finished(builder), "SelectAnimation");
    if (!reply) return Support::Unexpected(reply.error());
    auto message = Decode(*reply, "SelectAnimation");
    if (!message) return Support::Unexpected(message.error());
    const auto* loaded = (*message)->reply_as_Loaded();
    if (loaded == nullptr) {
        return Support::Unexpected(
            std::string("the host did not answer SelectAnimation with Loaded"));
    }
    return ReadLoaded(*loaded);
}

Support::Expected<Loaded, std::string> Host::ShowSymbol(const std::string& name) {
    flatbuffers::FlatBufferBuilder builder;
    const auto show = PreviewProtocol::CreateShowSymbol(builder, builder.CreateString(name));
    builder.Finish(PreviewProtocol::CreateRequestMessage(
        builder, PreviewProtocol::Request::ShowSymbol, show.Union()));
    auto reply = Call(Finished(builder), "ShowSymbol");
    if (!reply) return Support::Unexpected(reply.error());
    auto message = Decode(*reply, "ShowSymbol");
    if (!message) return Support::Unexpected(message.error());
    const auto* loaded = (*message)->reply_as_Loaded();
    if (loaded == nullptr)
        return Support::Unexpected(std::string("the host did not answer ShowSymbol with Loaded"));
    return ReadLoaded(*loaded);
}

Support::Expected<void, std::string> Host::Seek(uint32_t frame) {
    flatbuffers::FlatBufferBuilder builder;
    const auto seek = PreviewProtocol::CreateSeek(builder, frame);
    builder.Finish(PreviewProtocol::CreateRequestMessage(builder, PreviewProtocol::Request::Seek,
                                                         seek.Union()));
    auto reply = Call(Finished(builder), "Seek");
    if (!reply) return Support::Unexpected(reply.error());
    auto message = Decode(*reply, "Seek");
    if (!message) return Support::Unexpected(message.error());
    return {};
}

Support::Expected<void, std::string> Host::Resize(uint32_t width, uint32_t height) {
    flatbuffers::FlatBufferBuilder builder;
    const auto resize = PreviewProtocol::CreateResize(builder, width, height);
    builder.Finish(PreviewProtocol::CreateRequestMessage(builder, PreviewProtocol::Request::Resize,
                                                         resize.Union()));
    auto reply = Call(Finished(builder), "Resize");
    if (!reply) return Support::Unexpected(reply.error());
    auto message = Decode(*reply, "Resize");
    if (!message) return Support::Unexpected(message.error());
    return {};
}

Support::Expected<Frame, std::string> Host::Render() {
    flatbuffers::FlatBufferBuilder builder;
    const auto render = PreviewProtocol::CreateRender(builder);
    builder.Finish(PreviewProtocol::CreateRequestMessage(builder, PreviewProtocol::Request::Render,
                                                         render.Union()));
    auto reply = Call(Finished(builder), "Render");
    if (!reply) return Support::Unexpected(reply.error());
    auto message = Decode(*reply, "Render");
    if (!message) return Support::Unexpected(message.error());
    const auto* frame = (*message)->reply_as_Frame();
    if (frame == nullptr)
        return Support::Unexpected(std::string("the host did not answer Render with a Frame"));
    return Frame{.shared_handle = frame->shared_handle(),
                 .width = frame->width(),
                 .height = frame->height(),
                 .frame = frame->frame()};
}

}
