#include <catch2/catch_test_macros.hpp>

#include "editor_mime.h"
#include "editor_timeline.h"
#include "editor_timeline_metrics.h"
#include "widget_test_support.h"

#include "document/key_selection.h"
#include "document/outline.h"
#include "document/timeline.h"

#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QImage>
#include <QMimeData>
#include <QObject>
#include <QPointF>
#include <QScrollArea>
#include <QScrollBar>
#include <QString>

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

using namespace WidgetTest;

TEST_CASE("Pressing a bar chooses its depth, and only a click without a drag seeks") {
    Editor::Timeline timeline;
    timeline.resize(kTimelineWidth, 200);
    timeline.ShowAnimation(
        11,
        {Document::DepthRow{
            .depth = 3, .spans = {Document::Span{.first_frame = 1, .last_frame = 8}}, .shows = {}}},
        {});
    std::vector<uint32_t> depths;
    std::vector<uint32_t> frames;
    QObject::connect(&timeline, &Editor::Timeline::DepthChosen,
                     [&depths](uint32_t depth) { depths.push_back(depth); });
    QObject::connect(&timeline, &Editor::Timeline::FrameChosen,
                     [&frames](uint32_t frame) { frames.push_back(frame); });
    Drag(timeline, {FrameX(3), kDepthRowY}, {FrameX(5), kDepthRowY});
    CHECK_FALSE(depths.empty());
    CHECK(frames.empty());
    Click(timeline, {FrameX(6), kDepthRowY});
    CHECK(frames == std::vector<uint32_t>{6});
}

TEST_CASE("Holding Shift snaps a dragged bar's ends to other spans and the clip's marks") {
    constexpr uint32_t kFrames = 201;
    Editor::Timeline timeline;
    timeline.resize(kTimelineWidth, 200);
    timeline.ShowAnimation(
        kFrames,
        {Document::DepthRow{.depth = 3,
                            .spans = {Document::Span{.first_frame = 10, .last_frame = 40}},
                            .shows = {}},
         Document::DepthRow{.depth = 5,
                            .spans = {Document::Span{.first_frame = 60, .last_frame = 80}},
                            .shows = {}}},
        {Document::AnimationLabel{.name = "loop", .frame = 150}});
    timeline.SetFrame(100);
    const auto x_of = [](uint32_t frame) {
        return FrameX(0) + ((FrameX(10) - FrameX(0)) * frame / (kFrames - 1));
    };
    std::vector<int64_t> moves;
    std::vector<Document::Span> trims;
    QObject::connect(&timeline, &Editor::Timeline::SpanMoved,
                     [&moves](uint16_t, uint32_t, int64_t by) { moves.push_back(by); });
    QObject::connect(&timeline, &Editor::Timeline::SpanTrimmed,
                     [&trims](uint16_t, uint32_t, uint32_t first, uint32_t last) {
                         trims.push_back({.first_frame = first, .last_frame = last});
                     });
    Drag(timeline, {x_of(20), kDepthRowY}, {x_of(38), kDepthRowY});
    Drag(timeline, {x_of(20), kDepthRowY}, {x_of(38), kDepthRowY}, Qt::ShiftModifier);
    CHECK(moves == std::vector<int64_t>{18, 19});
    constexpr int kSecondRowY = kDepthRowY + 16;
    Drag(timeline, {x_of(70), kSecondRowY}, {x_of(88), kSecondRowY}, Qt::ShiftModifier);
    Drag(timeline, {x_of(70), kSecondRowY}, {x_of(138), kSecondRowY}, Qt::ShiftModifier);
    Drag(timeline, {x_of(70), kSecondRowY}, {x_of(189), kSecondRowY}, Qt::ShiftModifier);
    CHECK(moves == std::vector<int64_t>{18, 19, 19, 69, 120});
    const auto ghost_reaches = [&timeline](double x) {
        const QImage drawn = timeline.grab().toImage();
        for (int y = 40; y < 40 + 16; y++) {
            if (drawn.pixelColor(static_cast<int>(x), y) == QColor(240, 190, 80)) return true;
        }
        return false;
    };
    Send(timeline, QEvent::MouseButtonPress, {x_of(40), kDepthRowY}, Qt::LeftButton);
    Send(timeline, QEvent::MouseMove, {x_of(58), kDepthRowY}, Qt::LeftButton);
    CHECK_FALSE(ghost_reaches(x_of(59) - 1));
    Send(timeline, QEvent::MouseMove, {x_of(58), kDepthRowY}, Qt::LeftButton, Qt::ShiftModifier);
    CHECK(ghost_reaches(x_of(59) - 1));
    Send(timeline, QEvent::MouseButtonRelease, {x_of(58), kDepthRowY}, Qt::NoButton,
         Qt::ShiftModifier);
    Drag(timeline, {x_of(10), kDepthRowY}, {x_of(1), kDepthRowY}, Qt::ShiftModifier);
    CHECK(trims == std::vector<Document::Span>{{.first_frame = 10, .last_frame = 59},
                                               {.first_frame = 0, .last_frame = 40}});
}

TEST_CASE("The timeline zooms around the playhead from the keys and the cursor from the wheel") {
    QScrollArea area;
    auto* timeline = new Editor::Timeline;
    area.setWidget(timeline);
    area.setWidgetResizable(true);
    area.resize(kTimelineWidth + (2 * area.frameWidth()), 240);
    area.show();
    QApplication::processEvents();
    timeline->ZoomIn();
    timeline->ShowAnimation(201, {}, {});
    timeline->SetFrame(160);
    CHECK(timeline->minimumWidth() == 0);
    timeline->ZoomIn();
    CHECK(timeline->minimumWidth() > kTimelineWidth);
    CHECK(area.horizontalScrollBar()->value() > 0);
    timeline->ZoomOut();
    CHECK(timeline->minimumWidth() == 0);
    Wheel(*timeline, {FrameX(2), kDepthRowY}, 1, Qt::NoModifier);
    CHECK(timeline->minimumWidth() == 0);
    Wheel(*timeline, {FrameX(2), kDepthRowY}, 1, Qt::ControlModifier);
    CHECK(timeline->minimumWidth() > kTimelineWidth);
    Wheel(*timeline, {FrameX(2), kDepthRowY}, -1, Qt::ControlModifier);
    CHECK(timeline->minimumWidth() == 0);
}

TEST_CASE("A label dragged along the ruler asks to move there, and a click on it seeks") {
    Editor::Timeline timeline;
    timeline.resize(kTimelineWidth, 200);
    timeline.ShowAnimation(
        11,
        {Document::DepthRow{.depth = 2,
                            .spans = {Document::Span{.first_frame = 0, .last_frame = 10}},
                            .shows = {}}},
        {Document::AnimationLabel{.name = "loop", .frame = 3}});
    std::vector<std::pair<QString, uint32_t>> moved;
    std::vector<uint32_t> sought;
    QObject::connect(
        &timeline, &Editor::Timeline::LabelMoved,
        [&moved](const QString& label, uint32_t frame) { moved.emplace_back(label, frame); });
    QObject::connect(&timeline, &Editor::Timeline::FrameChosen,
                     [&sought](uint32_t frame) { sought.push_back(frame); });
    constexpr double kRulerY = 10;
    Send(timeline, QEvent::MouseButtonPress, {FrameX(3) + 2, kRulerY}, Qt::LeftButton);
    Send(timeline, QEvent::MouseMove, {FrameX(7), kRulerY}, Qt::LeftButton);
    const QImage drawn = timeline.grab().toImage();
    CHECK(drawn.pixelColor(static_cast<int>(FrameX(7)), 2) == QColor(220, 180, 90));
    CHECK(drawn.pixelColor(static_cast<int>(FrameX(3)), 2) != QColor(220, 180, 90));
    Send(timeline, QEvent::MouseButtonRelease, {FrameX(7), kRulerY}, Qt::NoButton);
    REQUIRE(moved.size() == 1);
    CHECK(moved[0].first == "loop");
    CHECK(moved[0].second == 7);
    CHECK(sought.empty());
    Drag(timeline, {FrameX(3), kRulerY}, {FrameX(3) + 20, kRulerY});
    Drag(timeline, {FrameX(3), kRulerY}, {FrameX(3) + 1, kRulerY});
    CHECK(moved.size() == 1);
    CHECK(sought == std::vector<uint32_t>{3});
    Click(timeline, {FrameX(5), kRulerY});
    CHECK(sought == std::vector<uint32_t>{3, 5});
    Drag(timeline, {FrameX(3) + 10, kRulerY}, {FrameX(8), kRulerY});
    CHECK(moved.size() == 1);
    CHECK(sought.back() == 8);
    Drag(timeline, {FrameX(3), kDepthRowY}, {FrameX(6), kDepthRowY});
    CHECK(moved.size() == 1);
}

TEST_CASE("A character dropped on the timeline is reported at its frame and row") {
    Editor::Timeline timeline;
    ShowScene(timeline);
    struct Dropped {
        uint16_t character = 0;
        uint32_t frame = 0;
        std::optional<uint16_t> depth;
    };
    std::vector<Dropped> drops;
    QObject::connect(&timeline, &Editor::Timeline::CharacterDropped,
                     [&drops](uint16_t character, uint32_t frame, std::optional<uint16_t> depth) {
                         drops.push_back({.character = character, .frame = frame, .depth = depth});
                     });
    const auto drop = [](Editor::Timeline& target, const QString& format, QPointF at,
                         const QByteArray& bytes = "12") {
        QMimeData data;
        data.setData(format, bytes);
        QDragEnterEvent enter(at.toPoint(), Qt::CopyAction, &data, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&target, &enter);
        QDropEvent dropped_on(at, Qt::CopyAction, &data, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&target, &dropped_on);
        return enter.isAccepted();
    };
    CHECK(drop(timeline, Editor::kCharacterMime, {FrameX(4), kDepthRowY}));
    CHECK(drop(timeline, Editor::kCharacterMime, {FrameX(7), kFirstPropertyY}));
    CHECK_FALSE(drop(timeline, Editor::kCharacterMime, {10, kDepthRowY}));
    CHECK_FALSE(drop(timeline, "text/plain", {FrameX(4), kDepthRowY}));
    drop(timeline, Editor::kCharacterMime, {FrameX(4), kDepthRowY}, "dot");
    Editor::Timeline empty;
    empty.resize(kTimelineWidth, 200);
    QObject::connect(&empty, &Editor::Timeline::CharacterDropped,
                     [&drops](uint16_t character, uint32_t frame, std::optional<uint16_t> depth) {
                         drops.push_back({.character = character, .frame = frame, .depth = depth});
                     });
    CHECK_FALSE(drop(empty, Editor::kCharacterMime, {FrameX(4), kDepthRowY}));
    REQUIRE(drops.size() == 2);
    CHECK(drops[0].character == 12);
    CHECK(drops[0].frame == 4);
    CHECK(drops[0].depth == uint16_t{1});
    CHECK(drops[1].frame == 7);
    CHECK_FALSE(drops[1].depth.has_value());
}

TEST_CASE("Alt-dragging the first or last selected keyframe stretches the selection") {
    Editor::Timeline timeline;
    ShowScene(timeline);
    std::vector<Document::KeyStretch> stretches;
    std::vector<int64_t> shifts;
    QObject::connect(
        &timeline, &Editor::Timeline::KeysStretched,
        [&stretches](const Document::KeyStretch& stretch) { stretches.push_back(stretch); });
    QObject::connect(&timeline, &Editor::Timeline::KeysShifted,
                     [&shifts](int64_t by) { shifts.push_back(by); });
    const auto is_selected_key = [&timeline](uint32_t frame) {
        const QColor drawn =
            timeline.grab().toImage().pixelColor(QPointF(FrameX(frame), kFirstPropertyY).toPoint());
        return drawn.red() > 200 && drawn.blue() < 150;
    };
    Click(timeline, {FrameX(0), kFirstPropertyY});
    Click(timeline, {FrameX(4), kFirstPropertyY}, Qt::ControlModifier);
    Click(timeline, {FrameX(8), kFirstPropertyY}, Qt::ControlModifier);

    Send(timeline, QEvent::MouseButtonPress, {FrameX(8), kFirstPropertyY}, Qt::LeftButton,
         Qt::AltModifier);
    Send(timeline, QEvent::MouseMove, {FrameX(4), kFirstPropertyY}, Qt::LeftButton,
         Qt::AltModifier);
    CHECK(is_selected_key(2));
    CHECK(is_selected_key(0));
    Send(timeline, QEvent::MouseButtonRelease, {FrameX(4), kFirstPropertyY}, Qt::NoButton,
         Qt::AltModifier);
    REQUIRE(stretches.size() == 1);
    CHECK(stretches[0].anchor == 0);
    CHECK(stretches[0].scale == 4);
    CHECK(stretches[0].over == 8);

    Drag(timeline, {FrameX(0), kFirstPropertyY}, {FrameX(2), kFirstPropertyY}, Qt::AltModifier);
    REQUIRE(stretches.size() == 2);
    CHECK(stretches[1].anchor == 8);
    CHECK(stretches[1].scale == -6);
    CHECK(stretches[1].over == -8);

    Drag(timeline, {FrameX(4), kFirstPropertyY}, {FrameX(5), kFirstPropertyY}, Qt::AltModifier);
    CHECK(stretches.size() == 2);
    CHECK(shifts == std::vector<int64_t>{1});
    Click(timeline, {FrameX(8), kFirstPropertyY}, Qt::AltModifier);
    Drag(timeline, {FrameX(8), kFirstPropertyY}, {FrameX(9), kFirstPropertyY});
    CHECK(stretches.size() == 2);
    CHECK(shifts == std::vector<int64_t>{1, 1});
    Click(timeline, {FrameX(6), kFirstPropertyY});
    Click(timeline, {FrameX(4), kFirstPropertyY});
    REQUIRE(timeline.SelectedKeys().size() == 1);
    Drag(timeline, {FrameX(4), kFirstPropertyY}, {FrameX(6), kFirstPropertyY}, Qt::AltModifier);
    CHECK(stretches.size() == 2);
    CHECK(shifts == std::vector<int64_t>{1, 1, 2});
}

TEST_CASE("The gutter switches solo a depth and dragging a row moves it to another depth") {
    Editor::Timeline timeline;
    timeline.resize(kTimelineWidth, 200);
    timeline.ShowAnimation(
        11,
        {Document::DepthRow{.depth = 3,
                            .spans = {Document::Span{.first_frame = 0, .last_frame = 10}}},
         Document::DepthRow{.depth = 2,
                            .spans = {Document::Span{.first_frame = 0, .last_frame = 10}}}},
        {});

    std::vector<uint16_t> soloed;
    QObject::connect(&timeline, &Editor::Timeline::SoloToggled,
                     [&soloed](uint16_t depth) { soloed.push_back(depth); });
    std::vector<std::pair<uint16_t, uint16_t>> moved;
    QObject::connect(&timeline, &Editor::Timeline::DepthDragged,
                     [&moved](uint16_t depth, uint16_t onto) { moved.emplace_back(depth, onto); });

    Click(timeline, {33, kDepthRowY});
    REQUIRE(soloed.size() == 1);
    CHECK(soloed.front() == 3);

    Drag(timeline, {48, kDepthRowY}, {48, kDepthRowY + 16});
    REQUIRE(moved.size() == 1);
    CHECK(moved.front().first == 3);
    CHECK(moved.front().second == 2);

    moved.clear();
    Drag(timeline, {48, kDepthRowY}, {50, kDepthRowY + 2});
    CHECK(moved.empty());
}

TEST_CASE("The scripts lane marks the frames carrying one and its button asks for a camera") {
    Editor::Timeline timeline;
    ShowScene(timeline);
    constexpr int kNoteY = 33;
    const QImage plain = timeline.grab().toImage();
    timeline.SetFrameNotes({Document::FrameNote{.frame = 2, .script = true, .camera = false},
                            Document::FrameNote{.frame = 6, .script = false, .camera = true}});
    const QImage marked = timeline.grab().toImage();
    CHECK(marked != plain);
    CHECK(marked.pixelColor(static_cast<int>(FrameX(2)), kNoteY) == QColor(150, 200, 240));
    CHECK(marked.pixelColor(static_cast<int>(FrameX(6)) - 2, kNoteY - 2) == QColor(240, 190, 80));
    CHECK(marked.pixelColor(static_cast<int>(FrameX(4)), kNoteY) != QColor(150, 200, 240));

    std::vector<uint32_t> sought;
    int cameras = 0;
    QObject::connect(&timeline, &Editor::Timeline::FrameChosen,
                     [&sought](uint32_t frame) { sought.push_back(frame); });
    QObject::connect(&timeline, &Editor::Timeline::CameraAsked, [&cameras] { cameras++; });
    Click(timeline, {FrameX(6) + 2, kNoteY});
    CHECK(sought == std::vector<uint32_t>{6});
    Click(timeline, {FrameX(4), kNoteY});
    CHECK(sought == std::vector<uint32_t>{6});
    CHECK(cameras == 0);
    Click(timeline, {20, kNoteY});
    CHECK(cameras == 1);
    CHECK(sought == std::vector<uint32_t>{6});
}

TEST_CASE("Double-clicking the ruler asks for a label, and a flag asks to rename it") {
    Editor::Timeline timeline;
    ShowScene(timeline);
    timeline.ShowAnimation(
        11,
        {Document::DepthRow{.depth = 1,
                            .spans = {Document::Span{.first_frame = 0, .last_frame = 10}}}},
        {Document::AnimationLabel{.name = "loop", .frame = 3}});
    std::vector<uint32_t> asked;
    std::vector<QString> renamed;
    QObject::connect(&timeline, &Editor::Timeline::LabelAsked,
                     [&asked](uint32_t frame) { asked.push_back(frame); });
    QObject::connect(&timeline, &Editor::Timeline::LabelRenameAsked,
                     [&renamed](const QString& label) { renamed.push_back(label); });
    constexpr double kRulerY = 10;
    Send(timeline, QEvent::MouseButtonDblClick, {FrameX(7), kRulerY}, Qt::LeftButton);
    CHECK(asked == std::vector<uint32_t>{7});
    CHECK(renamed.empty());
    Send(timeline, QEvent::MouseButtonDblClick, {FrameX(3) + 1, kRulerY}, Qt::LeftButton);
    CHECK(asked == std::vector<uint32_t>{7});
    REQUIRE(renamed.size() == 1);
    CHECK(renamed.front() == "loop");
    Send(timeline, QEvent::MouseButtonDblClick, {FrameX(7), kDepthRowY}, Qt::LeftButton);
    CHECK(asked == std::vector<uint32_t>{7});
    CHECK(renamed.size() == 1);
}

TEST_CASE("The gutter's column headers ask to show and unlock every depth") {
    Editor::Timeline timeline;
    ShowScene(timeline);
    int shown = 0;
    int unlocked = 0;
    std::vector<uint32_t> sought;
    QObject::connect(&timeline, &Editor::Timeline::ShowAllAsked, [&shown] { shown++; });
    QObject::connect(&timeline, &Editor::Timeline::UnlockAllAsked, [&unlocked] { unlocked++; });
    QObject::connect(&timeline, &Editor::Timeline::FrameChosen,
                     [&sought](uint32_t frame) { sought.push_back(frame); });
    const QImage plain = timeline.grab().toImage();
    Click(timeline, {Editor::kEyeLeft + 4, 12});
    CHECK(shown == 1);
    CHECK(unlocked == 0);
    Click(timeline, {Editor::kLockLeft + 4, 12});
    CHECK(unlocked == 1);
    Click(timeline, {Editor::kNumberLeft + 4, 12});
    CHECK(shown == 1);
    CHECK(unlocked == 1);
    CHECK(sought.empty());
    timeline.SetHiddenDepths({1});
    CHECK(timeline.grab().toImage() != plain);
}

TEST_CASE("The timeline zoom is set in pixels a frame and says when it changed") {
    Editor::Timeline timeline;
    timeline.resize(kTimelineWidth, 200);
    timeline.ShowAnimation(
        2000,
        {Document::DepthRow{.depth = 1,
                            .spans = {Document::Span{.first_frame = 0, .last_frame = 1999}}}},
        {});
    int changes = 0;
    QObject::connect(&timeline, &Editor::Timeline::ZoomChanged, [&changes] { changes++; });
    CHECK(timeline.ZoomPixels() < 1.0);
    CHECK(timeline.minimumWidth() == 0);

    timeline.SetZoomPixels(20);
    CHECK(timeline.ZoomPixels() == 20);
    CHECK(timeline.minimumWidth() > 20 * 1999);
    CHECK(changes == 1);

    timeline.ZoomIn();
    CHECK(timeline.ZoomPixels() > 20);
    CHECK(changes == 2);

    timeline.SetZoomPixels(0.01);
    CHECK(timeline.minimumWidth() == 0);
    CHECK(changes == 3);
}
