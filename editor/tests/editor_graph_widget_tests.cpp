#include <catch2/catch_test_macros.hpp>

#include "editor_graph.h"
#include "widget_test_support.h"

#include "document/keyframes.h"

#include <QColor>
#include <QEvent>
#include <QImage>
#include <QObject>
#include <QPointF>
#include <QString>

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

using namespace WidgetTest;

namespace {

Document::Track Moving() {
    return Document::Track{
        .property = "Translation",
        .keys = {
            Document::Keyframe{
                .frame = 0, .value = {0, 0}, .ease = Document::Ease::Linear, .bezier = {}},
            Document::Keyframe{
                .frame = 10, .value = {100, -100}, .ease = Document::Ease::Linear, .bezier = {}}}};
}

}

TEST_CASE("Dragging a key sideways in the graph retimes it between its neighbours") {
    Editor::GraphEditor graph;
    graph.resize(400, 200);
    Document::Track track = Moving();
    track.keys.insert(track.keys.begin() + 1, Document::Keyframe{.frame = 5,
                                                                 .value = {50, -50},
                                                                 .ease = Document::Ease::Linear,
                                                                 .bezier = {}});
    graph.ShowTrack(track, 0, 10, 0);
    struct Moved {
        uint32_t from = 0;
        uint32_t to = 0;
        std::vector<int64_t> value;
    };
    std::vector<Moved> moves;
    QObject::connect(
        &graph, &Editor::GraphEditor::KeyMoved,
        [&moves](const QString&, uint32_t frame, uint32_t to_frame, std::vector<int64_t> value) {
            moves.push_back({.from = frame, .to = to_frame, .value = std::move(value)});
        });
    const double left = graph.KeyPoint(0, 0).value_or(QPointF()).x();
    const double right = graph.KeyPoint(10, 0).value_or(QPointF()).x();
    const auto x_of = [left, right](double frame) { return left + ((right - left) * frame / 10); };
    const QPointF key = graph.KeyPoint(5, 0).value_or(QPointF());

    Send(graph, QEvent::MouseButtonPress, key, Qt::LeftButton);
    Send(graph, QEvent::MouseMove, {x_of(8), key.y()}, Qt::LeftButton);
    CHECK(graph.KeyPoint(8, 0).has_value());
    CHECK_FALSE(graph.KeyPoint(5, 0).has_value());
    Send(graph, QEvent::MouseButtonRelease, {x_of(8), key.y()}, Qt::NoButton);
    Drag(graph, key, {x_of(12), key.y()});
    Drag(graph, key, {x_of(-3), key.y()});
    Drag(graph, key, {x_of(7), key.y() - 10}, Qt::ShiftModifier);
    Drag(graph, key, {x_of(5.7), key.y() - 40}, Qt::ShiftModifier);
    Drag(graph, key, {x_of(3), key.y() - 25});
    REQUIRE(moves.size() == 6);
    CHECK(moves[0].from == 5);
    CHECK(moves[0].to == 8);
    CHECK(moves[0].value == std::vector<int64_t>{50, -50});
    CHECK(moves[1].to == 9);
    CHECK(moves[2].to == 1);
    CHECK(moves[3].to == 7);
    CHECK(moves[3].value == std::vector<int64_t>{50, -50});
    CHECK(moves[4].to == 5);
    CHECK(moves[4].value.at(0) > 50);
    CHECK(moves[5].to == 3);
    CHECK(moves[5].value.at(0) > 50);
}

TEST_CASE("The graph draws each value of a track apart, and nothing without one") {
    Editor::GraphEditor graph;
    graph.resize(400, 200);
    graph.ShowTrack(std::nullopt, 0, 10, 0);
    CHECK_FALSE(graph.KeyPoint(0, 0).has_value());
    graph.ShowTrack(Moving(), 0, 10, 5);
    const std::optional<QPointF> first_up = graph.KeyPoint(10, 0);
    const std::optional<QPointF> second_down = graph.KeyPoint(10, 1);
    REQUIRE(first_up.has_value());
    REQUIRE(second_down.has_value());
    CHECK(first_up->y() < second_down->y());
    CHECK(first_up->x() > graph.KeyPoint(0, 0).value_or(QPointF()).x());
    CHECK_FALSE(graph.KeyPoint(5, 0).has_value());
    const QColor boxed = graph.grab().toImage().pixelColor((*first_up + QPointF(3, 0)).toPoint());
    CHECK(boxed.red() > 150);
    CHECK(boxed.green() < 150);
    const QPointF middle = (*first_up + graph.KeyPoint(0, 0).value_or(QPointF())) / 2;
    const QColor lined = graph.grab().toImage().pixelColor(middle.toPoint());
    CHECK(lined.red() > 150);
    CHECK(lined.green() < 150);
}

TEST_CASE("Dragging a key in the graph sets that one value of it") {
    Editor::GraphEditor graph;
    graph.resize(400, 200);
    graph.ShowTrack(Moving(), 0, 10, 0);
    std::vector<uint32_t> chosen;
    std::vector<std::vector<int64_t>> changed;
    std::vector<uint32_t> sought;
    QObject::connect(&graph, &Editor::GraphEditor::KeyChosen,
                     [&chosen](const QString& property, uint32_t frame) {
                         CHECK(property == "Translation");
                         chosen.push_back(frame);
                     });
    QObject::connect(
        &graph, &Editor::GraphEditor::KeyMoved,
        [&changed](const QString&, uint32_t frame, uint32_t to_frame, std::vector<int64_t> value) {
            CHECK(frame == 10);
            CHECK(to_frame == 10);
            changed.push_back(std::move(value));
        });
    QObject::connect(&graph, &Editor::GraphEditor::FrameChosen,
                     [&sought](uint32_t frame) { sought.push_back(frame); });
    const QPointF key = graph.KeyPoint(10, 0).value_or(QPointF());
    Send(graph, QEvent::MouseButtonPress, key, Qt::LeftButton, Qt::NoModifier);
    Send(graph, QEvent::MouseMove, key - QPointF(0, 30), Qt::LeftButton, Qt::NoModifier);
    CHECK(graph.KeyPoint(10, 0).value_or(QPointF()).y() < key.y() - 20);
    Send(graph, QEvent::MouseButtonRelease, key - QPointF(0, 30), Qt::NoButton, Qt::NoModifier);
    CHECK(chosen == std::vector<uint32_t>{10});
    REQUIRE(changed.size() == 1);
    CHECK(changed.front().at(0) > 100);
    CHECK(changed.front().at(1) == -100);

    const QPointF second = graph.KeyPoint(10, 1).value_or(QPointF());
    Drag(graph, second, second + QPointF(0, 20));
    REQUIRE(changed.size() == 2);
    CHECK(changed.back().at(0) == 100);
    CHECK(changed.back().at(1) < -100);

    Click(graph, graph.KeyPoint(0, 1).value_or(QPointF()));
    CHECK(chosen == std::vector<uint32_t>{10, 10, 0});
    CHECK(changed.size() == 2);
    Click(graph, QPointF(185, 4));
    CHECK(sought == std::vector<uint32_t>{5});
}
