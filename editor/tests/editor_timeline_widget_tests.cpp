#include <catch2/catch_test_macros.hpp>

#include "editor_mime.h"
#include "editor_timeline.h"
#include "widget_test_support.h"

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
        for (int y = 26; y < 26 + 16; y++) {
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
