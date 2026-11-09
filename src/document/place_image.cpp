#include "document/place_image.h"

#include "document/document.h"
#include "document/frame_edit.h"
#include "support/expected.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace Document {

Support::Expected<uint16_t, std::string> PlaceImage(File& file, std::string_view animation_path,
                                                    std::string_view image, const DepthSpan& span) {
    File edited = file;
    const auto shape = edited.AddImageShape(animation_path, image);
    if (!shape) return Support::Unexpected(shape.error());
    auto animation = edited.ReadAnimation(animation_path);
    if (!animation) return Support::Unexpected(animation.error());
    const auto placed =
        AddDepth(*animation, span.clip, span.depth, *shape, span.first_frame, span.last_frame);
    if (!placed) return Support::Unexpected(placed.error());
    const auto written = edited.WriteAnimation(animation_path, *animation);
    if (!written) return Support::Unexpected(written.error());
    file = std::move(edited);
    return *shape;
}

}
