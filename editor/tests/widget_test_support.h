#pragma once

#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QPointF>
#include <QWidget>

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

}
