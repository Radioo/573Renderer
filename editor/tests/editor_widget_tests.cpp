#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "editor_ease_dialog.h"
#include "editor_timeline.h"
#include "editor_viewport.h"

#include "document/key_selection.h"
#include "document/keyframes.h"
#include "document/stage_bounds.h"
#include "document/timeline.h"

#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QImage>
#include <QMouseEvent>
#include <QObject>
#include <QPointF>
#include <QSize>
#include <QtGlobal>

#include <cmath>
#include <cstdint>
#include <optional>
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
};

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

TEST_CASE("Dragging inside the selection moves it by the stage offset") {
    Editor::Viewport viewport;
    ShowStage(viewport);
    std::vector<QPointF> moves;
    QObject::connect(&viewport, &Editor::Viewport::Dragged,
                     [&moves](uint16_t depth, double dx, double dy) {
                         CHECK(depth == 5);
                         moves.emplace_back(dx, dy);
                     });
    Drag(viewport, {150, 100}, {200, 150});
    REQUIRE(moves.size() == 1);
    CHECK_THAT(moves[0].x(), WithinAbs(100, 1e-9));
    CHECK_THAT(moves[0].y(), WithinAbs(100, 1e-9));

    Drag(viewport, {400, 400}, {420, 420});
    CHECK(moves.size() == 1);
    Click(viewport, {150, 100});
    CHECK(moves.size() == 1);
}

TEST_CASE("Dragging a corner scales and the round handle turns") {
    Editor::Viewport viewport;
    ShowStage(viewport);
    std::vector<Reshape> reshapes;
    QObject::connect(&viewport, &Editor::Viewport::Reshaped,
                     [&reshapes](uint16_t depth, double sx, double sy, double turn) {
                         reshapes.push_back(
                             {.depth = depth, .scale_x = sx, .scale_y = sy, .turn = turn});
                     });
    Drag(viewport, {250, 150}, {350, 150});
    REQUIRE(reshapes.size() == 1);
    CHECK(reshapes[0].depth == 5);
    CHECK_THAT(reshapes[0].scale_x, WithinAbs(1.5, 1e-9));
    CHECK_THAT(reshapes[0].scale_y, WithinAbs(1.0, 1e-9));
    CHECK(reshapes[0].turn == 0);

    const QPointF handle(150, 22);
    const QPointF anchor(50, 50);
    const QPointF to(250, 50);
    Drag(viewport, handle, to);
    REQUIRE(reshapes.size() == 2);
    const double expected = std::atan2(to.y() - anchor.y(), to.x() - anchor.x()) -
                            std::atan2(handle.y() - anchor.y(), handle.x() - anchor.x());
    CHECK_THAT(reshapes[1].turn, WithinAbs(expected, 1e-9));
    CHECK(reshapes[1].scale_x == 1);
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
