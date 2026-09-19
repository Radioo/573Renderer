#pragma once

#include "editor_timeline.h"

#include "document/keyframes.h"
#include "document/timeline.h"

#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QPoint>
#include <QPointF>
#include <QWheelEvent>
#include <QWidget>

#include <cstdint>

namespace WidgetTest {

inline void Send(QWidget& widget, QEvent::Type type, QPointF at, Qt::MouseButtons held,
                 Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QMouseEvent event(type, at, widget.mapToGlobal(at),
                      type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, held, modifiers);
    QApplication::sendEvent(&widget, &event);
}

inline void Click(QWidget& widget, QPointF at, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    Send(widget, QEvent::MouseButtonPress, at, Qt::LeftButton, modifiers);
    Send(widget, QEvent::MouseButtonRelease, at, Qt::NoButton, modifiers);
}

inline void Drag(QWidget& widget, QPointF from, QPointF to,
                 Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    Send(widget, QEvent::MouseButtonPress, from, Qt::LeftButton, modifiers);
    Send(widget, QEvent::MouseMove, (from + to) / 2, Qt::LeftButton, modifiers);
    Send(widget, QEvent::MouseMove, to, Qt::LeftButton, modifiers);
    Send(widget, QEvent::MouseButtonRelease, to, Qt::NoButton, modifiers);
}

inline constexpr int kTimelineWidth = 657;
inline constexpr int kDepthRowY = 34;
inline constexpr int kFirstPropertyY = 50;
inline constexpr int kSecondPropertyY = 66;

inline double FrameX(uint32_t frame) {
    return 56.0 + (60.0 * frame);
}

inline Document::Keyframe Key(uint32_t frame) {
    return Document::Keyframe{
        .frame = frame, .value = {0}, .ease = Document::Ease::Linear, .bezier = {}};
}

inline void ShowScene(Editor::Timeline& timeline) {
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

inline void Wheel(QWidget& widget, QPointF at, int notches, Qt::KeyboardModifiers modifiers) {
    QWheelEvent event(at, widget.mapToGlobal(at), QPoint(), QPoint(0, 120 * notches), Qt::NoButton,
                      modifiers, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&widget, &event);
}

}
