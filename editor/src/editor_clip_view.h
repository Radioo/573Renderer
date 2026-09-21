#pragma once

#include "document/characters.h"
#include "document/clip.h"
#include "document/document.h"
#include "document/frame_notes.h"
#include "document/inputs.h"
#include "document/outline.h"
#include "document/timeline.h"

#include <QImage>
#include <QString>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Editor {

struct LibraryRow {
    uint16_t id = 0;
    QString label;
    Document::CharacterKind kind = Document::CharacterKind::Sprite;
    std::size_t uses = 0;
    QImage tile;
};

struct ClipView {
    bool read = false;
    bool has_clip = false;
    QString refusal;
    std::vector<Document::AnimationLabel> labels;
    double rate = 0;
    std::vector<LibraryRow> characters;
    std::map<uint16_t, QString> names;
    std::map<uint16_t, Document::CharacterKind> kinds;
    std::vector<Document::DepthRow> depths;
    std::map<uint16_t, std::vector<uint32_t>> marks;
    std::vector<Document::FrameNote> notes;
    Document::InputSurface inputs;
    uint32_t model_frames = 0;
};

[[nodiscard]] ClipView ReadClipView(Document::File& file, const std::string& animation_path,
                                    Document::ClipId clip);

}
