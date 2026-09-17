#include "editor_window.h"

#include "editor_timeline.h"
#include "editor_viewport.h"

#include "document/characters.h"
#include "document/clip.h"
#include "document/hidden_depths.h"
#include "document/outline.h"
#include "document/place_image.h"
#include "support/expected.h"
#include "document/sprite_exports.h"
#include "document/sprite_preview.h"

#include <QAction>
#include <QComboBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Editor {

QWidget* Window::BuildTimelinePanel(QScrollArea* timeline_area) {
    clip_box_ = new QComboBox;
    clip_box_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(clip_box_, &QComboBox::currentIndexChanged, this, &Window::ChooseClip);

    auto* header = new QHBoxLayout;
    header->setContentsMargins(4, 2, 4, 2);
    header->addWidget(new QLabel(tr("Clip")));
    header->addWidget(clip_box_);
    header->addStretch();

    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(header);
    layout->addWidget(timeline_area);
    return panel;
}

std::optional<Placeable> Window::ChoosePlaceable(const AfpAnimation::Animation& animation) {
    std::vector<Placeable> choices;
    QStringList labels;
    for (const Document::CharacterSummary& one :
         Document::Characters(animation, file_->ShapeImages(animation_path_))) {
        choices.push_back(Placeable{.character = one.id, .image = {}});
        labels.append(QString::fromStdString(one.label));
    }
    for (const Document::Node& node : file_->Nodes()) {
        for (const Document::Node& child : node.children) {
            if (child.role != Document::Role::Texture) continue;
            choices.push_back(Placeable{.character = std::nullopt, .image = child.name});
            labels.append(
                tr("Package image %1, as a new shape").arg(QString::fromStdString(child.name)));
        }
    }
    if (choices.empty()) {
        ReportProblem(tr("Nothing in this package can be placed"));
        return std::nullopt;
    }
    bool answered = false;
    const QString picked =
        QInputDialog::getItem(this, tr("Add a depth"), tr("Place"), labels, 0, false, &answered);
    if (!answered) return std::nullopt;
    const auto index = labels.indexOf(picked);
    if (index < 0) return std::nullopt;
    return choices[static_cast<std::size_t>(index)];
}

void Window::PlaceImage(const std::string& image, const Document::DepthSpan& span) {
    const std::string path = animation_path_;
    EditDocument(tr("Place %1 on depth %2").arg(QString::fromStdString(image)).arg(span.depth),
                 [path, image, span](Document::File& document) {
                     using Placed = Support::Expected<void, std::string>;
                     const auto shape = Document::PlaceImage(document, path, image, span);
                     if (!shape) return Placed(Support::Unexpected(shape.error()));
                     return Placed();
                 });
    ShowClipTimeline();
    ShowFrame();
}

void Window::FillClips() {
    clip_ = {};
    const QSignalBlocker blocked(clip_box_);
    clip_box_->clear();
    if (!file_ || animation_path_.empty()) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportOnce(QString::fromStdString(animation.error()));
        return;
    }
    for (const Document::ClipSummary& clip : Document::Clips(*animation)) {
        const QVariant sprite =
            clip.id.sprite ? QVariant(static_cast<int>(*clip.id.sprite)) : QVariant();
        clip_box_->addItem(QString::fromStdString(Document::ClipLabel(clip)), sprite);
    }
    clip_box_->setCurrentIndex(0);
}

void Window::RefillClipsKeepingChoice() {
    const Document::ClipId kept = clip_;
    FillClips();
    const QVariant wanted = kept.sprite ? QVariant(static_cast<int>(*kept.sprite)) : QVariant();
    const int index = clip_box_->findData(wanted);
    if (index < 0) return;
    const QSignalBlocker blocked(clip_box_);
    clip_box_->setCurrentIndex(index);
    clip_ = kept;
}

void Window::NameShownSpriteExport() {
    if (!file_ || animation_path_.empty() || !clip_.sprite) return;
    const uint16_t sprite = *clip_.sprite;
    const QString current =
        QString::fromStdString(Document::SpriteExportName(*file_, animation_path_, sprite));
    bool answered = false;
    const QString name =
        QInputDialog::getText(this, tr("Name the export"), tr("Export name (empty removes it)"),
                              QLineEdit::Normal, current, &answered);
    if (!answered || name == current) return;
    const std::string path = animation_path_;
    const std::string text = name.toStdString();
    if (!EditDocument(tr("Name sprite %1 %2").arg(sprite).arg(name),
                      [path, sprite, text](Document::File& document) {
                          return Document::NameSpriteExport(document, path, sprite, text);
                      })) {
        return;
    }
    RefillClipsKeepingChoice();
}

void Window::ChooseClip(int index) {
    if (index < 0) return;
    const QVariant sprite = clip_box_->itemData(index);
    clip_ = sprite.isValid() ? Document::ClipId{.sprite = static_cast<uint16_t>(sprite.toInt())}
                             : Document::ClipId{};
    StopPlayback();
    depth_.reset();
    key_property_.clear();
    key_frame_.reset();
    frame_ = clip_.sprite ? 0 : root_frame_;
    if (file_) LoadViewportClip(*file_);
    ShowClipTimeline();
    SeekViewport(frame_);
    ShowFrame();
    if (!clip_.sprite) return;
    if (symbol_shown_) {
        statusBar()->showMessage(tr("Showing %1 on its own").arg(clip_box_->itemText(index)));
        return;
    }
    statusBar()->showMessage(tr("Editing %1. The viewport still shows the root animation, at "
                                "frame %2.")
                                 .arg(clip_box_->itemText(index))
                                 .arg(root_frame_));
}

bool Window::LoadViewportClip(const Document::File& document) {
    symbol_shown_ = false;
    if (!host_.Running() || animation_path_.empty()) return false;
    std::optional<Document::File> view;
    if (!hidden_.empty()) {
        auto filtered = Document::ViewWithout(document, hidden_);
        if (!filtered) {
            ReportOnce(QString::fromStdString(filtered.error()));
            return false;
        }
        view = std::move(*filtered);
    }
    const Document::File& file = view ? *view : document;
    std::vector<uint8_t> bytes;
    std::string symbol;
    if (clip_.sprite) {
        auto preview = Document::PreviewSymbolFor(file, animation_path_, clip_);
        if (!preview) {
            ReportOnce(QString::fromStdString(preview.error()));
            return false;
        }
        bytes = std::move(preview->ifs);
        symbol = std::move(preview->name);
    } else {
        auto encoded = file.Encode();
        if (!encoded) {
            ReportOnce(QString::fromStdString(encoded.error()));
            return false;
        }
        bytes = std::move(*encoded);
    }
    const auto loaded = host_.Reload(package_name_, animation_name_, bytes);
    if (!loaded) {
        ReportOnce(QString::fromStdString(loaded.error()));
        return false;
    }
    frame_count_ = loaded->frame_count;
    if (symbol.empty()) return true;
    const auto shown = host_.ShowSymbol(symbol);
    if (!shown) {
        ReportOnce(QString::fromStdString(shown.error()));
        return false;
    }
    frame_count_ = shown->frame_count;
    symbol_shown_ = true;
    return true;
}

void Window::ShowClipTimeline() {
    if (!file_ || animation_path_.empty()) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportOnce(QString::fromStdString(animation.error()));
        return;
    }
    const std::optional<Document::AnimationDetails> details =
        Document::DescribeClip(*animation, clip_);
    if (!details) {
        FillClips();
        ShowClipTimeline();
        return;
    }
    const bool from_model = !host_.Running() || (clip_.sprite && !symbol_shown_);
    const uint32_t count = from_model ? details->frame_count : frame_count_;
    timeline_->ShowAnimation(count, details->depths, details->labels);
    UpdateViewRows();
    frame_ = count == 0 ? 0 : std::min(frame_, count - 1);
    timeline_->SetFrame(frame_);
}

void Window::ShowAnimation(const std::string& name) {
    StopPlayback();
    animation_name_ = name;
    frame_ = 0;
    root_frame_ = 0;
    symbol_shown_ = false;
    depth_.reset();
    if (!host_.Running()) {
        viewport_->ShowMessage(
            tr("Choose a game install to preview %1").arg(QString::fromStdString(name)));
        frame_count_ = 0;
        FillClips();
        ShowClipTimeline();
        ShowFrame();
        return;
    }
    const auto encoded = file_->Encode();
    if (!encoded) {
        ReportOnce(QString::fromStdString(encoded.error()));
        return;
    }
    const auto loaded = host_.ShowAnimation(package_name_, name, *encoded);
    if (!loaded) {
        ReportOnce(QString::fromStdString(loaded.error()));
        return;
    }
    frame_count_ = loaded->frame_count;
    FillClips();
    ShowClipTimeline();
    ResizeViewport();
}

void Window::SeekViewport(uint32_t frame) {
    if (!clip_.sprite) root_frame_ = frame;
    if (!host_.Running()) return;
    const auto sought = host_.Seek(frame);
    if (!sought) {
        ReportOnce(QString::fromStdString(sought.error()));
        StopPlayback();
        return;
    }
    RenderFrame();
}

void Window::SeekTo(uint32_t frame) {
    frame_ = frame;
    timeline_->SetFrame(frame);
    if (!clip_.sprite || symbol_shown_) SeekViewport(frame);
    if (!Playing()) ShowFrame();
}

void Window::Reload() {
    if (animation_name_.empty() || !file_) return;
    LoadViewportClip(*file_);
    ShowClipTimeline();
    SeekViewport(symbol_shown_ ? frame_ : root_frame_);
    ShowFrame();
}

}
