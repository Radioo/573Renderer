#pragma once

#include <QImage>
#include <QString>
#include <QWidget>

class QPaintEvent;
class QResizeEvent;

namespace Editor {

class Viewport : public QWidget {
    Q_OBJECT

public:
    explicit Viewport(QWidget* parent = nullptr);

    void ShowFrame(const QImage& frame);
    void ShowMessage(const QString& message);

signals:
    void Resized(int width, int height);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QImage frame_;
    QString message_;
};

}
