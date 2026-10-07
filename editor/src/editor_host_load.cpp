#include "editor_host_load.h"

#include "document/outline.h"
#include "document/sprite_preview.h"
#include "document/timeline.h"

#include <algorithm>
#include <optional>
#include <utility>

namespace Editor {

namespace {

bool PlacedOnRoot(const AfpAnimation::Animation& animation, const Document::ClipId& clip,
                  uint32_t root_frame) {
    if (!clip.sprite) return false;
    const AfpAnimation::Container* root = Document::FindClip(animation, Document::ClipId{});
    if (root == nullptr) return false;
    for (const Document::DepthRow& row : Document::DepthRows(*root)) {
        for (const auto& [at, character] : row.shows) {
            if (at <= root_frame && character == *clip.sprite) return true;
        }
    }
    return false;
}

}

HostLoad LoadIntoHost(Host& host, const HostRequest& asked) {
    HostLoad load;
    std::optional<Document::File> view;
    if (!asked.hidden.empty()) {
        auto filtered = Document::ViewWithout(asked.document, asked.hidden);
        if (!filtered) {
            load.refusal = QString::fromStdString(filtered.error());
            return load;
        }
        view = std::move(*filtered);
    }
    const Document::File& file = view ? *view : asked.document;
    Document::File reading = asked.document;
    const auto animation = reading.ReadAnimation(asked.animation_path);
    const bool in_place = asked.clip.sprite && asked.wants_in_place && animation &&
                          PlacedOnRoot(*animation, asked.clip, asked.root_frame);

    std::vector<uint8_t> bytes;
    std::string symbol;
    if (asked.clip.sprite && !in_place) {
        auto preview = Document::PreviewSymbolFor(file, asked.animation_path, asked.clip);
        if (!preview) {
            load.refusal = QString::fromStdString(preview.error());
            return load;
        }
        bytes = std::move(preview->ifs);
        symbol = std::move(preview->name);
    } else {
        auto encoded = file.Encode();
        if (!encoded) {
            load.refusal = QString::fromStdString(encoded.error());
            return load;
        }
        bytes = std::move(*encoded);
    }

    const auto loaded = asked.fresh
                            ? host.ShowAnimation(asked.package_name, asked.animation_name, bytes)
                            : host.Reload(asked.package_name, asked.animation_name, bytes);
    if (!loaded) {
        load.refusal = QString::fromStdString(loaded.error());
        return load;
    }
    load.frame_count = loaded->frame_count;
    load.loaded = true;
    if (symbol.empty()) return load;
    const auto shown = host.ShowSymbol(symbol);
    if (!shown) {
        load.refusal = QString::fromStdString(shown.error());
        return load;
    }
    load.frame_count = shown->frame_count;
    load.symbol_shown = true;
    return load;
}

}
