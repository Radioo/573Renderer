#include "editor_clip_view.h"

#include "document/playback.h"

#include <algorithm>
#include <optional>
#include <utility>

namespace Editor {

namespace {

constexpr int kTileSide = 32;

QImage TileFor(Document::File& file, const std::map<uint16_t, std::string>& images,
               uint16_t character) {
    const auto named = images.find(character);
    if (named == images.end()) return {};
    const auto pixels = file.ReadImage(named->second);
    if (!pixels) return {};
    const QImage picture(pixels->bgra.data(), static_cast<int>(pixels->width),
                         static_cast<int>(pixels->height), QImage::Format_ARGB32);
    return picture.scaled(kTileSide, kTileSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

}

ClipView ReadClipView(Document::File& file, const std::string& animation_path,
                      Document::ClipId clip) {
    ClipView view;
    const auto animation = file.ReadAnimation(animation_path);
    if (!animation) {
        view.refusal = QString::fromStdString(animation.error());
        return view;
    }
    view.read = true;
    const std::optional<Document::AnimationDetails> details =
        Document::DescribeClip(*animation, clip);
    if (!details) return view;
    view.has_clip = true;
    view.labels = details->labels;
    view.rate = Document::FrameRate(*animation);
    view.model_frames = details->frame_count;

    const std::map<uint16_t, std::string> images = file.ShapeImages(animation_path);
    const std::map<uint16_t, std::size_t> uses = Document::CharacterUses(*animation);
    for (const Document::CharacterSummary& one : Document::Characters(*animation, images)) {
        const auto found = uses.find(one.id);
        view.characters.push_back(LibraryRow{.id = one.id,
                                             .label = QString::fromStdString(one.label),
                                             .kind = one.kind,
                                             .uses = found == uses.end() ? 0 : found->second,
                                             .tile = TileFor(file, images, one.id)});
        view.names.emplace(one.id, QString::fromStdString(one.label));
        view.kinds.emplace(one.id, one.kind);
    }

    view.depths = details->depths;
    std::ranges::sort(view.depths, std::ranges::greater{}, &Document::DepthRow::depth);
    if (const AfpAnimation::Container* shown = Document::FindClip(*animation, clip)) {
        for (const Document::DepthRow& row : view.depths)
            view.marks.emplace(row.depth, Document::DepthMarks(*shown, row.depth));
        view.notes = Document::FrameNotes(*shown);
    }
    return view;
}

}
