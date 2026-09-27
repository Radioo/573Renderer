#pragma once

#include <QPixmap>
#include <QRect>
#include <QString>
#include <QStyledItemDelegate>

namespace Editor::Rows {

inline constexpr int kDetailRole = Qt::UserRole + 20;
inline constexpr int kHeaderRole = Qt::UserRole + 21;
inline constexpr int kChipRole = Qt::UserRole + 22;
inline constexpr int kOpenRole = Qt::UserRole + 23;
inline constexpr int kThumbWidth = 52;
inline constexpr int kThumbHeight = 29;
inline constexpr int kHeight = 40;
inline constexpr int kHeaderHeight = 28;

[[nodiscard]] QPixmap Stripes(int width, int height, int seed);

class Delegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit Delegate(QObject* parent = nullptr);

    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const override;
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

private:
    void PaintHeader(QPainter* painter, const QRect& rect, const QString& text) const;
    void PaintChip(QPainter* painter, const QRect& box, const QString& text) const;
    void PaintMark(QPainter* painter, const QRect& rect, bool open) const;
};

}
