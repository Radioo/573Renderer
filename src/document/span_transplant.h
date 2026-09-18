#pragma once

#include "document/clip.h"
#include "document/document.h"
#include "document/span_clipboard.h"
#include "document/timeline.h"
#include "support/expected.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace Document {

[[nodiscard]] Support::Expected<CopiedSpan, std::string>
CopySpanFrom(const File& file, std::string_view animation_path, ClipId clip, uint16_t depth,
             uint32_t frame);

[[nodiscard]] Support::Expected<Span, std::string>
PasteSpanInto(File& file, std::string_view animation_path, ClipId clip, const CopiedSpan& copied,
              uint16_t depth, uint32_t first_frame);

}
