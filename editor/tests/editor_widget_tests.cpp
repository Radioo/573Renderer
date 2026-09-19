#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "editor_ease_dialog.h"
#include "editor_timeline.h"
#include "editor_mime.h"
#include "editor_viewport.h"

#include "document/key_selection.h"
#include "document/keyframes.h"
#include "document/stage_bounds.h"
#include "document/timeline.h"

#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QByteArray>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QSize>
#include <QtGlobal>

#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace {

using Catch::Matchers::WithinAbs;

constexpr int kTimelineWidth = 657;
constexpr int kDepthRowY = 34;
constexpr int kFirstPropertyY = 50;
constexpr int kSecondPropertyY = 66;

void Send(QWidget& widget, QEvent::Type type, QPointF at, Qt::MouseButtons held,
          Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QMouseEvent event(type, at, widget.mapToGlobal(at),
                      type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, held, modifiers);
    QApplication::sendEvent(&widget, &event);
}

void Click(QWidget& widget, QPointF at, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    Send(widget, QEvent::MouseButtonPress, at, Qt::LeftButton, modifiers);
    Send(widget, QEvent::MouseButtonRelease, at, Qt::NoButton, modifiers);
}

void Drag(QWidget& widget, QPointF from, QPointF to,
          Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    Send(widget, QEvent::MouseButtonPress, from, Qt::LeftButton, modifiers);
    Send(widget, QEvent::MouseMove, (from + to) / 2, Qt::LeftButton, modifiers);
    Send(widget, QEvent::MouseMove, to, Qt::LeftButton, modifiers);
    Send(widget, QEvent::MouseButtonRelease, to, Qt::NoButton, modifiers);
}

double FrameX(uint32_t frame) {
    return 56.0 + (60.0 * frame);
}

Document::Keyframe Key(uint32_t frame) {
    return Document::Keyframe{
        .frame = frame, .value = {0}, .ease = Document::Ease::Linear, .bezier = {}};
}

void ShowScene(Editor::Timeline& timeline) {
    timeline.resize(kTimelineWidth, 200);
    timeline.ShowAnimation(
        11,
        {Document::DepthRow{.depth = 1,
                            .spans = {Document::Span{.first_frame = 0, .last_frame = 10}}}},
        {});
    timeline.ShowKeys(uint16_t{1},
                      {Document::Track{.property = "Translation", .keys = {Key(0), Key(4), Key(8)}},
                       Document::Track{.property = "Multiply colour", .keys = {Key(2)}}});
}

Document::KeyRef Ref(const char* property, uint32_t frame) {
    return Document::KeyRef{.property = property, .frame = frame};
}

void Wheel(QWidget& widget, QPointF at, int notches, Qt::KeyboardModifiers modifiers) {
    QWheelEvent event(at, widget.mapToGlobal(at), QPoint(), QPoint(0, 120 * notches), Qt::NoButton,
                      modifiers, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&widget, &event);
}

void MiddleDrag(QWidget& widget, QPointF from, QPointF to) {
    QMouseEvent press(QEvent::MouseButtonPress, from, widget.mapToGlobal(from), Qt::MiddleButton,
                      Qt::MiddleButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press);
    QMouseEvent move(QEvent::MouseMove, to, widget.mapToGlobal(to), Qt::NoButton, Qt::MiddleButton,
                     Qt::NoModifier);
    QApplication::sendEvent(&widget, &move);
    QMouseEvent release(QEvent::MouseButtonRelease, to, widget.mapToGlobal(to), Qt::MiddleButton,
                        Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &release);
}

std::vector<QPointF> PickedAt(Editor::Viewport& viewport, const std::vector<QPointF>& points) {
    std::vector<QPointF> picked;
    const QMetaObject::Connection connection =
        QObject::connect(&viewport, &Editor::Viewport::Picked,
                         [&picked](double x, double y) { picked.emplace_back(x, y); });
    for (const QPointF& point : points)
        Click(viewport, point);
    QObject::disconnect(connection);
    return picked;
}

void ShowStage(Editor::Viewport& viewport) {
    viewport.resize(960, 540);
    QImage frame(1920, 1080, QImage::Format_ARGB32);
    frame.fill(QColor(0, 0, 0));
    viewport.ShowFrame(frame, QSize(1920, 1080));
    viewport.ShowOutlines(
        {Document::StageOutline{.depth = 5,
                                .corners = {Document::Point{100, 100}, Document::Point{500, 100},
                                            Document::Point{500, 300}, Document::Point{100, 300}},
                                .anchor = {100, 100},
                                .linear = {}}},
        uint16_t{5});
}

struct Reshape {
    uint16_t depth = 0;
    double scale_x = 0;
    double scale_y = 0;
    double turn = 0;
    bool finished = false;
};

struct Move {
    QPointF by;
    bool finished = false;
};

template <typename T> std::vector<T> Finished(const std::vector<T>& all) {
    std::vector<T> done;
    for (const T& one : all) {
        if (one.finished) done.push_back(one);
    }
    return done;
}

}

TEST_CASE("Clicking a keyframe selects it alone and focuses it") {
    Editor::Timeline timeline;
    ShowScene(timeline);
    std::vector<uint32_t> focused;
    QObject::connect(&timeline, &Editor::Timeline::KeyChosen,
                     [&focused](const QString&, uint32_t frame) { focused.push_back(frame); });
    Click(timeline, {FrameX(4), kFirstPropertyY});
    CHECK(timeline.SelectedKeys() == std::vector<Document::KeyRef>{Ref("Translation", 4)});
    CHECK(focused == std::vector<uint32_t>{4});
    Click(timeline, {FrameX(8), kFirstPropertyY});
    CHECK(timeline.SelectedKeys() == std::vector<Document::KeyRef>{Ref("Translation", 8)});
}

TEST_CASE("Ctrl-clicking adds and drops keyframes from the selection") {
    Editor::Timeline timeline;
    ShowScene(timeline);
    Click(timeline, {FrameX(4), kFirstPropertyY});
    Click(timeline, {FrameX(2), kSecondPropertyY}, Qt::ControlModifier);
    CHECK(timeline.SelectedKeys() ==
          std::vector<Document::KeyRef>{Ref("Translation", 4), Ref("Multiply colour", 2)});
    Click(timeline, {FrameX(4), kFirstPropertyY}, Qt::ControlModifier);
    CHECK(timeline.SelectedKeys() == std::vector<Document::KeyRef>{Ref("Multiply colour", 2)});
}

TEST_CASE("A box drawn across the lanes selects the keyframes it touches") {
    Editor::Timeline timeline;
    ShowScene(timeline);
    Click(timeline, {FrameX(0), kFirstPropertyY});
    Drag(timeline, {FrameX(1) + 10, kFirstPropertyY}, {FrameX(9), kSecondPropertyY});
    CHECK(timeline.SelectedKeys().size() == 3);
    CHECK(std::ranges::find(timeline.SelectedKeys(), Ref("Translation", 0)) ==
          timeline.SelectedKeys().end());

    Drag(timeline, {FrameX(10), kFirstPropertyY}, {FrameX(10) - 10, kFirstPropertyY});
    CHECK(timeline.SelectedKeys().empty());
}

TEST_CASE("A box drawn with Ctrl adds to the selection") {
    Editor::Timeline timeline;
    ShowScene(timeline);
    Click(timeline, {FrameX(0), kFirstPropertyY});
    Drag(timeline, {FrameX(1), kSecondPropertyY}, {FrameX(3), kSecondPropertyY},
         Qt::ControlModifier);
    CHECK(timeline.SelectedKeys() ==
          std::vector<Document::KeyRef>{Ref("Translation", 0), Ref("Multiply colour", 2)});
    Click(timeline, {FrameX(6), kFirstPropertyY}, Qt::ControlModifier);
    CHECK(timeline.SelectedKeys().size() == 2);
    Click(timeline, {FrameX(6), kFirstPropertyY});
    CHECK(timeline.SelectedKeys().empty());
}

TEST_CASE("Dragging a selected keyframe asks to move the selection by whole frames") {
    Editor::Timeline timeline;
    ShowScene(timeline);
    std::vector<int64_t> moves;
    QObject::connect(&timeline, &Editor::Timeline::KeysShifted,
                     [&moves](int64_t by) { moves.push_back(by); });
    Click(timeline, {FrameX(4), kFirstPropertyY});
    Click(timeline, {FrameX(8), kFirstPropertyY}, Qt::ControlModifier);
    Drag(timeline, {FrameX(4), kFirstPropertyY}, {FrameX(6) + 3, kFirstPropertyY});
    CHECK(moves == std::vector<int64_t>{2});
    CHECK(timeline.SelectedKeys().size() == 2);
    Click(timeline, {FrameX(4), kFirstPropertyY});
    CHECK(moves.size() == 1);
}

TEST_CASE("Dragging a depth's bar asks to move that span by whole frames") {
    Editor::Timeline timeline;
    timeline.resize(kTimelineWidth, 200);
    timeline.ShowAnimation(
        11,
        {Document::DepthRow{.depth = 3,
                            .spans = {Document::Span{.first_frame = 1, .last_frame = 4},
                                      Document::Span{.first_frame = 7, .last_frame = 9}}}},
        {});
    struct Moved {
        uint16_t depth = 0;
        uint32_t frame = 0;
        int64_t by = 0;
    };
    std::vector<Moved> moves;
    QObject::connect(&timeline, &Editor::Timeline::SpanMoved,
                     [&moves](uint16_t depth, uint32_t frame, int64_t by) {
                         moves.push_back({.depth = depth, .frame = frame, .by = by});
                     });
    Drag(timeline, {FrameX(8), kDepthRowY}, {FrameX(6), kDepthRowY});
    REQUIRE(moves.size() == 1);
    CHECK(moves[0].depth == 3);
    CHECK(moves[0].frame == 8);
    CHECK(moves[0].by == -2);

    Drag(timeline, {FrameX(2), kDepthRowY}, {FrameX(2) + 2, kDepthRowY});
    Drag(timeline, {FrameX(5) + 20, kDepthRowY}, {FrameX(9), kDepthRowY});
    CHECK(moves.size() == 1);
}

TEST_CASE("A hidden depth's bars are drawn grey") {
    Editor::Timeline timeline;
    timeline.resize(kTimelineWidth, 200);
    timeline.ShowAnimation(
        11,
        {Document::DepthRow{.depth = 3,
                            .spans = {Document::Span{.first_frame = 1, .last_frame = 9}}}},
        {});
    const QPoint inside(static_cast<int>(FrameX(4)), kDepthRowY);
    CHECK(timeline.grab().toImage().pixelColor(inside) == QColor(70, 128, 196));
    timeline.SetHiddenDepths({3});
    CHECK(timeline.grab().toImage().pixelColor(inside) == QColor(92, 92, 98));
    timeline.SetHiddenDepths({});
    CHECK(timeline.grab().toImage().pixelColor(inside) == QColor(70, 128, 196));
}

TEST_CASE("A span's bar shows the name of what it places, and says it on hover") {
    Editor::Timeline timeline;
    timeline.resize(kTimelineWidth, 200);
    timeline.ShowAnimation(
        11,
        {Document::DepthRow{.depth = 3,
                            .spans = {Document::Span{.first_frame = 1, .last_frame = 9}},
                            .shows = {{1U, uint16_t{12}}}},
         Document::DepthRow{.depth = 4,
                            .spans = {Document::Span{.first_frame = 2, .last_frame = 2}},
                            .shows = {{2U, uint16_t{12}}}}},
        {});
    const auto lettering = [&timeline](int y) {
        const QImage image = timeline.grab().toImage();
        int bright = 0;
        for (int x = static_cast<int>(FrameX(1)); x < static_cast<int>(FrameX(9)); x++) {
            for (int dy = -4; dy <= 4; dy++) {
                const QColor colour = image.pixelColor(x, y + dy);
                if (colour.red() > 180 && colour.green() > 180 && colour.blue() > 180) bright++;
            }
        }
        return bright;
    };
    CHECK(lettering(kDepthRowY) == 0);
    timeline.SetCharacterNames({{uint16_t{12}, QString("Sprite 12: banner")}});
    CHECK(lettering(kDepthRowY) > 20);
    CHECK(lettering(kDepthRowY + 16) == 0);

    CHECK(timeline.SpanNameAt(QPoint(static_cast<int>(FrameX(5)), kDepthRowY)) ==
          "Sprite 12: banner");
    CHECK(timeline.SpanNameAt(QPoint(static_cast<int>(FrameX(2)), kDepthRowY + 16)) ==
          "Sprite 12: banner");
    CHECK(timeline.SpanNameAt(QPoint(static_cast<int>(FrameX(10)), kDepthRowY)).isEmpty());
    CHECK(timeline.SpanNameAt(QPoint(static_cast<int>(FrameX(5)), 4)).isEmpty());
}

TEST_CASE("The gutter's eye and lock switch a depth without choosing it or a frame") {
    Editor::Timeline timeline;
    timeline.resize(kTimelineWidth, 200);
    timeline.ShowAnimation(
        11,
        {Document::DepthRow{.depth = 3,
                            .spans = {Document::Span{.first_frame = 1, .last_frame = 9}}}},
        {});
    std::vector<uint16_t> seen;
    std::vector<uint16_t> locked;
    std::vector<uint32_t> chosen;
    QObject::connect(&timeline, &Editor::Timeline::VisibilityToggled,
                     [&seen](uint16_t depth) { seen.push_back(depth); });
    QObject::connect(&timeline, &Editor::Timeline::LockToggled,
                     [&locked](uint16_t depth) { locked.push_back(depth); });
    QObject::connect(&timeline, &Editor::Timeline::DepthChosen,
                     [&chosen](uint32_t depth) { chosen.push_back(depth); });
    Click(timeline, QPointF(9, kDepthRowY));
    CHECK(seen == std::vector<uint16_t>{3});
    CHECK(locked.empty());
    CHECK(chosen.empty());
    Click(timeline, QPointF(23, kDepthRowY));
    CHECK(locked == std::vector<uint16_t>{3});
    CHECK(chosen.empty());
    Click(timeline, QPointF(45, kDepthRowY));
    CHECK(chosen == std::vector<uint32_t>{3});
    CHECK(seen.size() == 1);
    CHECK(locked.size() == 1);

    const QRect gutter(0, kDepthRowY - 6, 56, 12);
    const QImage plain = timeline.grab(gutter).toImage();
    timeline.SetHiddenDepths({3});
    CHECK(timeline.grab(gutter).toImage() != plain);
}

TEST_CASE("A locked depth is marked in the gutter") {
    Editor::Timeline timeline;
    timeline.resize(kTimelineWidth, 200);
    timeline.ShowAnimation(
        11,
        {Document::DepthRow{.depth = 3,
                            .spans = {Document::Span{.first_frame = 1, .last_frame = 9}}}},
        {});
    const QRect gutter(0, kDepthRowY - 6, 56, 12);
    const QImage plain = timeline.grab(gutter).toImage();
    timeline.SetLockedDepths({3});
    CHECK(timeline.grab(gutter).toImage() != plain);
    timeline.SetLockedDepths({});
    CHECK(timeline.grab(gutter).toImage() == plain);
}

TEST_CASE("The work area is shaded on the ruler") {
    Editor::Timeline timeline;
    ShowScene(timeline);
    const QPoint inside(static_cast<int>(FrameX(3)) + 5, 3);
    const QColor plain = timeline.grab().toImage().pixelColor(inside);
    timeline.SetWorkArea(Document::WorkArea{.first_frame = 2, .last_frame = 4});
    CHECK(timeline.grab().toImage().pixelColor(inside) == QColor(72, 96, 132));
    CHECK(timeline.grab().toImage().pixelColor(QPoint(static_cast<int>(FrameX(6)) + 5, 3)) ==
          plain);
    timeline.SetWorkArea(std::nullopt);
    CHECK(timeline.grab().toImage().pixelColor(inside) == plain);
}

TEST_CASE("Dragging a bar's edge asks to trim that span") {
    Editor::Timeline timeline;
    timeline.resize(kTimelineWidth, 200);
    timeline.ShowAnimation(
        11,
        {Document::DepthRow{.depth = 3,
                            .spans = {Document::Span{.first_frame = 1, .last_frame = 4},
                                      Document::Span{.first_frame = 7, .last_frame = 9}}}},
        {});
    std::vector<std::vector<uint32_t>> trims;
    int moves = 0;
    QObject::connect(&timeline, &Editor::Timeline::SpanTrimmed,
                     [&trims](uint16_t depth, uint32_t frame, uint32_t first, uint32_t last) {
                         trims.push_back({depth, frame, first, last});
                     });
    QObject::connect(&timeline, &Editor::Timeline::SpanMoved,
                     [&moves](uint16_t, uint32_t, int64_t) { moves++; });
    Drag(timeline, {FrameX(4), kDepthRowY}, {FrameX(6), kDepthRowY});
    Drag(timeline, {FrameX(7) + 1, kDepthRowY}, {FrameX(5), kDepthRowY});
    Drag(timeline, {FrameX(9), kDepthRowY}, {FrameX(2), kDepthRowY});
    REQUIRE(trims.size() == 3);
    CHECK(trims[0] == std::vector<uint32_t>{3, 1, 1, 6});
    CHECK(trims[1] == std::vector<uint32_t>{3, 7, 5, 9});
    CHECK(trims[2] == std::vector<uint32_t>{3, 7, 7, 7});
    CHECK(moves == 0);
}

TEST_CASE("A selection loses keyframes that are gone and all of it when the depth changes") {
    Editor::Timeline timeline;
    ShowScene(timeline);
    timeline.SelectKeys({Ref("Translation", 4), Ref("Translation", 8)});
    timeline.ShowKeys(uint16_t{1},
                      {Document::Track{.property = "Translation", .keys = {Key(0), Key(8)}}});
    CHECK(timeline.SelectedKeys() == std::vector<Document::KeyRef>{Ref("Translation", 8)});
    timeline.ShowKeys(uint16_t{2},
                      {Document::Track{.property = "Translation", .keys = {Key(0), Key(8)}}});
    CHECK(timeline.SelectedKeys().empty());
    CHECK_FALSE(timeline.grab().isNull());
}

TEST_CASE("Ghosts are drawn faintly over the frame until the next frame arrives") {
    Editor::Viewport viewport;
    ShowStage(viewport);
    const QPoint middle(480, 270);
    const QImage plain = viewport.grab().toImage();
    QImage white(1920, 1080, QImage::Format_ARGB32);
    white.fill(QColor(255, 255, 255));
    viewport.ShowGhosts({white});
    const QColor ghosted = viewport.grab().toImage().pixelColor(middle);
    CHECK(ghosted.red() > 70);
    CHECK(ghosted.red() < 110);
    QImage frame(1920, 1080, QImage::Format_ARGB32);
    frame.fill(QColor(0, 0, 0));
    viewport.ShowFrame(frame, QSize(1920, 1080));
    CHECK(viewport.grab().toImage() == plain);
}

TEST_CASE("Clicking the stage reports where in stage pixels") {
    Editor::Viewport viewport;
    ShowStage(viewport);
    std::vector<QPointF> picked;
    QObject::connect(&viewport, &Editor::Viewport::Picked,
                     [&picked](double x, double y) { picked.emplace_back(x, y); });
    Click(viewport, {600, 400});
    REQUIRE(picked.size() == 1);
    CHECK_THAT(picked[0].x(), WithinAbs(1200, 1e-9));
    CHECK_THAT(picked[0].y(), WithinAbs(800, 1e-9));
    CHECK(viewport.FittedSize(QSize(1000, 1000)) == QSize(1000, 562));
    CHECK_FALSE(viewport.grab().isNull());
}

TEST_CASE("Ctrl and the wheel zoom the stage around the cursor, and fitting undoes it") {
    Editor::Viewport viewport;
    ShowStage(viewport);
    int zoomed = 0;
    QObject::connect(&viewport, &Editor::Viewport::ZoomChanged, [&zoomed] { zoomed++; });
    Wheel(viewport, {600, 400}, 1, Qt::NoModifier);
    CHECK(zoomed == 0);
    CHECK(PickedAt(viewport, {{0, 0}}) == std::vector<QPointF>{{0, 0}});

    Wheel(viewport, {600, 400}, 2, Qt::ControlModifier);
    CHECK(zoomed == 1);
    const std::vector<QPointF> picked = PickedAt(viewport, {{600, 400}, {0, 0}});
    REQUIRE(picked.size() == 2);
    CHECK_THAT(picked[0].x(), WithinAbs(1200, 1e-6));
    CHECK_THAT(picked[0].y(), WithinAbs(800, 1e-6));
    CHECK_THAT(picked[1].x(), WithinAbs(1200 - (1200 / 1.5625), 1e-6));
    CHECK_THAT(picked[1].y(), WithinAbs(800 - (800 / 1.5625), 1e-6));
    CHECK(viewport.FittedSize(QSize(960, 540)) == QSize(1500, 844));

    Wheel(viewport, {600, 400}, 8, Qt::ControlModifier);
    CHECK(viewport.FittedSize(QSize(960, 540)) == QSize(1920, 1080));

    viewport.FitStage();
    CHECK(zoomed == 3);
    CHECK(PickedAt(viewport, {{600, 400}}) == std::vector<QPointF>{{1200, 800}});
    CHECK(viewport.FittedSize(QSize(960, 540)) == QSize(960, 540));
}

TEST_CASE("Dragging with the middle button pans the stage") {
    Editor::Viewport viewport;
    ShowStage(viewport);
    std::vector<Move> moves;
    QObject::connect(&viewport, &Editor::Viewport::Dragged,
                     [&moves](uint16_t, double dx, double dy, bool finished) {
                         moves.push_back({.by = QPointF(dx, dy), .finished = finished});
                     });
    MiddleDrag(viewport, {150, 100}, {250, 150});
    CHECK(moves.empty());
    CHECK(PickedAt(viewport, {{250, 150}}) == std::vector<QPointF>{{300, 200}});
    viewport.FitStage();
    CHECK(PickedAt(viewport, {{300, 250}}) == std::vector<QPointF>{{600, 500}});
}

TEST_CASE("A character dropped on the stage is reported at its stage point") {
    Editor::Viewport viewport;
    ShowStage(viewport);
    std::vector<std::pair<uint16_t, QPointF>> dropped;
    QObject::connect(&viewport, &Editor::Viewport::CharacterDropped,
                     [&dropped](uint16_t character, double x, double y) {
                         dropped.emplace_back(character, QPointF(x, y));
                     });
    const auto drop = [&viewport](const QString& format, const QByteArray& bytes) {
        QMimeData data;
        data.setData(format, bytes);
        QDragEnterEvent enter(QPoint(300, 200), Qt::CopyAction, &data, Qt::LeftButton,
                              Qt::NoModifier);
        QApplication::sendEvent(&viewport, &enter);
        QDropEvent dropped_on(QPointF(300, 200), Qt::CopyAction, &data, Qt::LeftButton,
                              Qt::NoModifier);
        QApplication::sendEvent(&viewport, &dropped_on);
        return enter.isAccepted();
    };
    CHECK(drop(Editor::kCharacterMime, "12"));
    REQUIRE(dropped.size() == 1);
    CHECK(dropped[0].first == 12);
    CHECK(dropped[0].second == QPointF(600, 400));
    CHECK_FALSE(drop("text/plain", "12"));
    CHECK(dropped.size() == 1);
}

TEST_CASE("A guide pulled from the ruler is snapped to until it goes back or is cleared") {
    Editor::Viewport viewport;
    ShowStage(viewport);
    viewport.SetSnapping(true);
    std::vector<QPointF> picked;
    QObject::connect(&viewport, &Editor::Viewport::Picked,
                     [&picked](double x, double y) { picked.emplace_back(x, y); });
    std::vector<Move> moves;
    QObject::connect(&viewport, &Editor::Viewport::Dragged,
                     [&moves](uint16_t, double dx, double dy, bool finished) {
                         moves.push_back({.by = QPointF(dx, dy), .finished = finished});
                     });
    const auto moved_by = [&] {
        moves.clear();
        Drag(viewport, {150, 100}, {150, 148});
        REQUIRE_FALSE(moves.empty());
        return moves.back().by.y();
    };
    CHECK_THAT(moved_by(), WithinAbs(96, 1e-9));
    const auto moved_up = [&] {
        moves.clear();
        Drag(viewport, {150, 100}, {150, 57});
        REQUIRE_FALSE(moves.empty());
        return moves.back().by.y();
    };
    CHECK_THAT(moved_up(), WithinAbs(-86, 1e-9));

    picked.clear();
    Click(viewport, {300, 5});
    CHECK(picked.size() == 1);
    viewport.SetRulers(true);
    picked.clear();
    Drag(viewport, {300, 5}, {300, 200});
    CHECK(picked.empty());
    CHECK(viewport.grab().toImage().pixelColor(700, 200) == QColor(0, 200, 230));
    CHECK_THAT(moved_by(), WithinAbs(100, 1e-9));

    Drag(viewport, {300, 200}, {300, 5});
    CHECK_THAT(moved_by(), WithinAbs(96, 1e-9));
    CHECK_THAT(moved_up(), WithinAbs(-86, 1e-9));

    Drag(viewport, {300, 5}, {300, 200});
    CHECK_THAT(moved_by(), WithinAbs(100, 1e-9));
    viewport.ClearGuides();
    CHECK_THAT(moved_by(), WithinAbs(96, 1e-9));
}

TEST_CASE("Dragging inside the selection moves it by the stage offset") {
    Editor::Viewport viewport;
    ShowStage(viewport);
    std::vector<Move> moves;
    QObject::connect(&viewport, &Editor::Viewport::Dragged,
                     [&moves](uint16_t depth, double dx, double dy, bool finished) {
                         CHECK(depth == 5);
                         moves.push_back({.by = QPointF(dx, dy), .finished = finished});
                     });
    Drag(viewport, {150, 100}, {200, 150});
    REQUIRE(moves.size() == 3);
    CHECK_FALSE(moves[0].finished);
    CHECK_THAT(moves[0].by.x(), WithinAbs(50, 1e-9));
    CHECK_FALSE(moves[1].finished);
    CHECK(moves[2].finished);
    CHECK_THAT(moves[2].by.x(), WithinAbs(100, 1e-9));
    CHECK_THAT(moves[2].by.y(), WithinAbs(100, 1e-9));
    CHECK(moves[1].by == moves[2].by);

    Drag(viewport, {400, 400}, {420, 420});
    CHECK(moves.size() == 3);
    Click(viewport, {150, 100});
    CHECK(moves.size() == 3);
}

TEST_CASE("Arrow keys nudge the selection by one stage pixel, or ten with Shift") {
    Editor::Viewport viewport;
    ShowStage(viewport);
    viewport.SetSnapping(true);
    std::vector<Move> moves;
    QObject::connect(&viewport, &Editor::Viewport::Dragged,
                     [&moves](uint16_t depth, double dx, double dy, bool finished) {
                         CHECK(depth == 5);
                         moves.push_back({.by = QPointF(dx, dy), .finished = finished});
                     });
    const auto press = [&viewport](int key, Qt::KeyboardModifiers modifiers) {
        QKeyEvent event(QEvent::KeyPress, key, modifiers);
        QApplication::sendEvent(&viewport, &event);
        return event.isAccepted();
    };
    CHECK(press(Qt::Key_Left, Qt::NoModifier));
    CHECK(press(Qt::Key_Down, Qt::ShiftModifier));
    CHECK_FALSE(press(Qt::Key_A, Qt::NoModifier));
    REQUIRE(moves.size() == 2);
    CHECK(moves[0].finished);
    CHECK(moves[0].by == QPointF(-1, 0));
    CHECK(moves[1].finished);
    CHECK(moves[1].by == QPointF(0, 10));
    CHECK(viewport.focusPolicy() == Qt::StrongFocus);

    viewport.ShowOutlines({}, std::nullopt);
    CHECK_FALSE(press(Qt::Key_Right, Qt::NoModifier));
    CHECK(moves.size() == 2);
}

TEST_CASE("A snapping drag lands on the stage edge and shows its guide unless Alt is held") {
    Editor::Viewport viewport;
    ShowStage(viewport);
    viewport.SetSnapping(true);
    std::vector<Move> moves;
    QObject::connect(&viewport, &Editor::Viewport::Dragged,
                     [&moves](uint16_t, double dx, double dy, bool finished) {
                         moves.push_back({.by = QPointF(dx, dy), .finished = finished});
                     });
    Send(viewport, QEvent::MouseButtonPress, {150, 100}, Qt::LeftButton);
    Send(viewport, QEvent::MouseMove, {104, 100}, Qt::LeftButton);
    const QImage during = viewport.grab().toImage();
    CHECK(during.pixelColor(0, 10) == QColor(255, 80, 200));
    Send(viewport, QEvent::MouseButtonRelease, {104, 100}, Qt::NoButton);
    REQUIRE(Finished(moves).size() == 1);
    CHECK_THAT(Finished(moves)[0].by.x(), WithinAbs(-100, 1e-9));
    CHECK_THAT(Finished(moves)[0].by.y(), WithinAbs(0, 1e-9));
    CHECK(viewport.grab().toImage().pixelColor(0, 10) != QColor(255, 80, 200));

    moves.clear();
    Drag(viewport, {150, 100}, {104, 100}, Qt::AltModifier);
    REQUIRE(Finished(moves).size() == 1);
    CHECK_THAT(Finished(moves)[0].by.x(), WithinAbs(-92, 1e-9));
}

TEST_CASE("Dragging a corner scales and the round handle turns") {
    Editor::Viewport viewport;
    ShowStage(viewport);
    std::vector<Reshape> reshapes;
    QObject::connect(
        &viewport, &Editor::Viewport::Reshaped,
        [&reshapes](uint16_t depth, double sx, double sy, double turn, bool finished) {
            reshapes.push_back(
                {.depth = depth, .scale_x = sx, .scale_y = sy, .turn = turn, .finished = finished});
        });
    Drag(viewport, {250, 150}, {350, 150});
    REQUIRE(reshapes.size() == 3);
    CHECK_FALSE(reshapes[0].finished);
    CHECK_THAT(reshapes[0].scale_x, WithinAbs(1.25, 1e-9));
    const std::vector<Reshape> scaled = Finished(reshapes);
    REQUIRE(scaled.size() == 1);
    CHECK(scaled[0].depth == 5);
    CHECK_THAT(scaled[0].scale_x, WithinAbs(1.5, 1e-9));
    CHECK_THAT(scaled[0].scale_y, WithinAbs(1.0, 1e-9));
    CHECK(scaled[0].turn == 0);

    const QPointF handle(150, 22);
    const QPointF anchor(50, 50);
    const QPointF to(250, 50);
    Drag(viewport, handle, to);
    const std::vector<Reshape> turned = Finished(reshapes);
    REQUIRE(turned.size() == 2);
    const double expected = std::atan2(to.y() - anchor.y(), to.x() - anchor.x()) -
                            std::atan2(handle.y() - anchor.y(), handle.x() - anchor.x());
    CHECK_THAT(turned[1].turn, WithinAbs(expected, 1e-9));
    CHECK(turned[1].scale_x == 1);
}

TEST_CASE("Dragging an ease handle moves it and keeps it inside the segment's time") {
    Editor::CurveEditor curve;
    curve.resize(328, 328);
    curve.SetCurve({.x1 = 0.25, .y1 = 0.0, .x2 = 0.75, .y2 = 1.0});
    int edits = 0;
    QObject::connect(&curve, &Editor::CurveEditor::CurveEdited, [&edits] { edits++; });
    Drag(curve, {24 + 70, 24 + 210}, {24 + 400, 24 + 70});
    CHECK(edits == 2);
    CHECK(curve.Curve().x1 == 1.0);
    CHECK_THAT(curve.Curve().y1, WithinAbs(1.0, 1e-9));
    CHECK(curve.Curve().x2 == 0.75);

    Drag(curve, {10, 10}, {100, 100});
    CHECK(edits == 2);
    CHECK_FALSE(curve.grab().isNull());

    Editor::EaseDialog dialog({.x1 = 0.1, .y1 = 0.2, .x2 = 0.3, .y2 = 0.4}, nullptr);
    CHECK(dialog.Result() == Document::Bezier{.x1 = 0.1, .y1 = 0.2, .x2 = 0.3, .y2 = 0.4});
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "minimal");
    const QApplication app(argc, argv);
    return Catch::Session().run(argc, argv);
}
