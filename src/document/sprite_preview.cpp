#include "document/sprite_preview.h"

#include "document/animation_strings.h"
#include "document/clip.h"
#include "document/document.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Document {

namespace {

constexpr std::string_view kPreviewPrefix = "r573_preview_sprite_";

bool Exported(const AfpAnimation::Animation& animation, std::string_view name) {
    const std::string wanted = FoldedName(name);
    return std::ranges::any_of(animation.exports, [&](const AfpAnimation::Export& exported) {
        return FoldedName(StringText(animation, exported.name)) == wanted;
    });
}

std::string UnusedName(const AfpAnimation::Animation& animation, uint16_t sprite) {
    std::string name = std::string(kPreviewPrefix) + std::to_string(sprite);
    while (Exported(animation, name))
        name += "_";
    return name;
}

}

Support::Expected<PreviewSymbol, std::string>
PreviewSymbolFor(const File& file, std::string_view animation_path, ClipId clip) {
    if (!clip.sprite) return Support::Unexpected(std::string("the root clip is not a symbol"));
    auto animation = file.ReadAnimation(animation_path);
    if (!animation) return Support::Unexpected(animation.error());
    if (FindClip(*animation, clip) == nullptr) return Support::Unexpected(MissingClipMessage(clip));

    const auto exported =
        std::ranges::find(animation->exports, *clip.sprite, &AfpAnimation::Export::tag);
    if (exported != animation->exports.end()) {
        auto bytes = file.Encode();
        if (!bytes) return Support::Unexpected(bytes.error());
        return PreviewSymbol{.ifs = std::move(*bytes),
                             .name = StringText(*animation, exported->name)};
    }

    const std::string name = UnusedName(*animation, *clip.sprite);
    InsertExport(*animation, *clip.sprite, name);
    File preview = file;
    auto written = preview.WriteAnimation(animation_path, *animation);
    if (!written) return Support::Unexpected(written.error());
    auto bytes = preview.Encode();
    if (!bytes) return Support::Unexpected(bytes.error());
    return PreviewSymbol{.ifs = std::move(*bytes), .name = name};
}

}
