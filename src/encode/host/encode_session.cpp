#include "encode/host/encode_session.h"

#include "encode/frame_section.h"
#include "encode_host_generated.h"
#include "media/media_format.h"
#include "media_sink.h"
#include "video_encoder.h"

#include <flatbuffers/buffer.h>
#include <flatbuffers/flatbuffer_builder.h>
#include <flatbuffers/verifier.h>

#include <cstdint>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace EncodeHost {

namespace {

std::vector<uint8_t> Finished(const flatbuffers::FlatBufferBuilder& builder) {
    const std::span<const uint8_t> bytes(builder.GetBufferPointer(), builder.GetSize());
    return {bytes.begin(), bytes.end()};
}

template <typename T>
std::vector<uint8_t> Reply(flatbuffers::FlatBufferBuilder& builder, EncodeProtocol::Reply type,
                           flatbuffers::Offset<T> body) {
    builder.Finish(EncodeProtocol::CreateReplyMessage(builder, type, body.Union()));
    return Finished(builder);
}

std::vector<uint8_t> Failure(const std::string& message) {
    flatbuffers::FlatBufferBuilder builder;
    return Reply(builder, EncodeProtocol::Reply::Failure,
                 EncodeProtocol::CreateFailureDirect(builder, message.c_str()));
}

std::vector<uint8_t> Done() {
    flatbuffers::FlatBufferBuilder builder;
    return Reply(builder, EncodeProtocol::Reply::Done, EncodeProtocol::CreateDone(builder));
}

MediaSink::Params ParamsOf(const EncodeProtocol::Open& open) {
    MediaSink::Params p;
    p.output_path = open.output_path() != nullptr ? open.output_path()->str() : std::string();
    p.format = MediaSink::FromIndex(open.format());
    p.src_width = open.src_width();
    p.src_height = open.src_height();
    p.out_width = open.out_width();
    p.out_height = open.out_height();
    p.fps = open.fps();
    p.quality = open.quality();
    p.keyframe_interval = open.keyframe_interval();
    p.prefer_hardware = open.prefer_hardware();
    return p;
}

}

Session::Output* Session::Find(uint32_t session) {
    const auto found = outputs_.find(session);
    return found != outputs_.end() ? found->second.get() : nullptr;
}

std::vector<uint8_t> Session::Handle(std::span<const uint8_t> request) {
    flatbuffers::Verifier verifier(request.data(), request.size());
    if (!verifier.VerifyBuffer<EncodeProtocol::RequestMessage>(nullptr))
        return Failure("the request is not a valid message");
    const auto* message = flatbuffers::GetRoot<EncodeProtocol::RequestMessage>(request.data());

    if (const auto* open = message->request_as_Open()) {
        if (Find(open->session()) != nullptr)
            return Failure(std::format("session {} is already open", open->session()));
        const MediaSink::Params p = ParamsOf(*open);
        auto frames = EncodeFrames::Section::Open(open->frames() != nullptr ? open->frames()->str()
                                                                            : std::string());
        if (!frames) return Failure(frames.error());
        auto output = std::make_unique<Output>();
        output->frames = std::move(*frames);
        if (!output->sink.Open(p)) return Failure(output->sink.LastError());
        const bool hardware = output->sink.UsingHardware();
        outputs_[open->session()] = std::move(output);
        flatbuffers::FlatBufferBuilder builder;
        return Reply(builder, EncodeProtocol::Reply::Opened,
                     EncodeProtocol::CreateOpened(builder, hardware));
    }
    if (const auto* frame = message->request_as_Frame()) {
        Output* output = Find(frame->session());
        if (output == nullptr) return Failure(std::format("no session {}", frame->session()));
        if (!output->sink.SubmitFrame(output->frames->Data(), frame->index()))
            return Failure(output->sink.LastError());
        return Done();
    }
    if (const auto* finish = message->request_as_FinishOutput()) {
        Output* output = Find(finish->session());
        if (output == nullptr) return Failure(std::format("no session {}", finish->session()));
        const bool finished = output->sink.Finish();
        const std::string error = output->sink.LastError();
        outputs_.erase(finish->session());
        return finished ? Done() : Failure(error);
    }
    if (const auto* cancel = message->request_as_Cancel()) {
        Output* output = Find(cancel->session());
        if (output != nullptr) output->sink.Cancel();
        outputs_.erase(cancel->session());
        return Done();
    }
    if (const auto* probe = message->request_as_Probe()) {
        flatbuffers::FlatBufferBuilder builder;
        const bool available =
            VideoEncoder::HardwareAvailable(MediaSink::FromIndex(probe->format()));
        return Reply(builder, EncodeProtocol::Reply::Probed,
                     EncodeProtocol::CreateProbed(builder, available));
    }
    return Failure("unknown request");
}

}
