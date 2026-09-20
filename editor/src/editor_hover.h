#pragma once

#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QPoint>
#include <QPointF>
#include <QWidget>

namespace Editor {

inline void Hover(QWidget& widget) {
    widget.setAttribute(Qt::WA_Hover, true);
    widget.setAttribute(Qt::WA_UnderMouse, true);
    QEvent entered(QEvent::Enter);
    QApplication::sendEvent(&widget, &entered);
    const QPointF middle(widget.width() / 2.0, widget.height() / 2.0);
    const QPointF onScreen = widget.mapToGlobal(middle.toPoint());
    QMouseEvent moved(QEvent::MouseMove, middle, onScreen, Qt::NoButton, Qt::NoButton,
                      Qt::NoModifier);
    QApplication::sendEvent(&widget, &moved);
    widget.update();
    QApplication::processEvents();
}

inline void Unhover(QWidget& widget) {
    widget.setAttribute(Qt::WA_UnderMouse, false);
    QEvent left(QEvent::Leave);
    QApplication::sendEvent(&widget, &left);
    widget.update();
    QApplication::processEvents();
}

}
