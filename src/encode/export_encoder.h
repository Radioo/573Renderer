#pragma once

#include "media/media_format.h"

#if defined(R573_REMOTE_ENCODER)
#include "encode/encode_client.h"
#else
#include "media_sink.h"
#include "video_encoder.h"
#endif

namespace ExportEncoder {

#if defined(R573_REMOTE_ENCODER)
using Sink = EncodeClient::Sink;

[[nodiscard]] inline bool HardwareAvailable(MediaSink::Format probe_format) {
    return EncodeClient::HardwareAvailable(probe_format);
}
#else
using Sink = MediaSink::Sink;

[[nodiscard]] inline bool HardwareAvailable(MediaSink::Format probe_format) {
    return VideoEncoder::HardwareAvailable(probe_format);
}
#endif

}
