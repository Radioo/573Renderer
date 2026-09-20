#include "editor_rows.h"

#include "editor_theme.h"

#include <QFont>
#include <QIcon>
#include <QPainter>
#include <QPen>
#include <QRect>
#include <QString>

namespace Editor::Rows {

namespace {

constexpr int kLeft = 8;
constexpr int kGap = 9;
constexpr int kNameSize = 12;
constexpr int kDetailSize = 11;
constexpr int kStripeStep = 7;
constexpr int kStripeWidth = 2;
constexpr QColor kStripeBack(0x2f, 0x55, 0x7c);
constexpr QColor kStripeLine(0x5d, 0x8f, 0xc4);

}

QPixmap Stripes(int width, int height, int seed) {
    QPixmap drawn(width, height);
    drawn.fill(kStripeBack);
    QPainter painter(&drawn);
    const int lean = height / 2 + (seed % 3) * (height / 4);
    QColor line = kStripeLine;
    line.setAlpha(0x55);
    painter.setPen(QPen(line, kStripeWidth));
    for (int at = -height; at < width + height; at += kStripeStep)
        painter.drawLine(at, height, at + lean, 0);
    line.setAlpha(0x88);
    painter.setPen(QPen(line, 1));
    painter.drawRect(0, 0, width - 1, height - 1);
    return drawn;
}

Delegate::Delegate(QObject* parent) : QStyledItemDelegate(parent) {}

QSize Delegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    return {QStyledItemDelegate::sizeHint(option, index).width(), kHeight};
}

void Delegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                     const QModelIndex& index) const {
    const bool chosen = (option.state & QStyle::State_Selected) != 0;
    const bool under = (option.state & QStyle::State_MouseOver) != 0;
    painter->save();
    painter->fillRect(option.rect,
                      chosen ? Theme::kChosen : (under ? Theme::kField : Theme::kPanel));
    QRect left = option.rect.adjusted(kLeft, 0, 0, 0);
    const QPixmap thumbnail =
        qvariant_cast<QIcon>(index.data(Qt::DecorationRole)).pixmap(kThumbWidth, kThumbHeight);
    if (!thumbnail.isNull()) {
        const int top = left.top() + (left.height() - thumbnail.height()) / 2;
        painter->drawPixmap(left.left(), top, thumbnail);
        left.setLeft(left.left() + kThumbWidth + kGap);
    }
    const QString detail = index.data(kDetailRole).toString();
    QFont named(Theme::SansFamily());
    named.setPixelSize(kNameSize);
    named.setBold(chosen);
    painter->setFont(named);
    painter->setPen(chosen ? Theme::kText : Theme::kSoft);
    const QString name = painter->fontMetrics().elidedText(index.data(Qt::DisplayRole).toString(),
                                                           Qt::ElideRight, left.width() - kLeft);
    if (detail.isEmpty()) {
        painter->drawText(left, Qt::AlignLeft | Qt::AlignVCenter, name);
        painter->restore();
        return;
    }
    QRect top = left;
    top.setHeight(left.height() / 2);
    painter->drawText(top, Qt::AlignLeft | Qt::AlignBottom, name);
    QFont said(Theme::SansFamily());
    said.setPixelSize(kDetailSize);
    painter->setFont(said);
    painter->setPen(Theme::kFaint);
    QRect under_name = left;
    under_name.setTop(top.bottom() + 1);
    painter->drawText(
        under_name, Qt::AlignLeft | Qt::AlignTop,
        painter->fontMetrics().elidedText(detail, Qt::ElideRight, under_name.width() - kLeft));
    painter->restore();
}

}
