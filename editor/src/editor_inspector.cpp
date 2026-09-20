#include "editor_inspector.h"

#include "editor_mime.h"

#include "document/blend_modes.h"

#include "document/inspector_view.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>
#include <cstdint>
#include <limits>
#include <tuple>
#include <cstddef>

namespace Editor {

namespace {

constexpr int kLabelWidth = 86;
constexpr int kScrubStep = 4;
constexpr int kSwatch = 20;
constexpr double kColourHighest = 255.0;

QString UnitSuffix(Document::ViewUnit unit) {
    switch (unit) {
    case Document::ViewUnit::Pixels:
        return QStringLiteral(" px");
    case Document::ViewUnit::Percent:
        return QStringLiteral(" %");
    case Document::ViewUnit::Degrees:
        return QStringLiteral(" deg");
    case Document::ViewUnit::Colour:
    case Document::ViewUnit::Channels:
        return {};
    }
    return {};
}

QStringList AxisNames(const Document::ViewRow& row) {
    if (row.values.size() == 4)
        return {QStringLiteral("R"), QStringLiteral("G"), QStringLiteral("B"), QStringLiteral("A")};
    if (row.values.size() != 2) return {QString()};
    return {QStringLiteral("X"), QStringLiteral("Y")};
}

QString HexOf(const std::vector<double>& values) {
    QString hex;
    for (std::size_t i = 0; i + 1 < values.size(); i++)
        hex += QString("%1").arg(static_cast<int>(values[i]), 2, 16, QChar('0')).toUpper();
    return hex;
}

QWidget* Row(QWidget* left, QWidget* right) {
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(left);
    layout->addWidget(right, 1);
    return row;
}

QWidget* Section(const QString& title, QVBoxLayout*& body) {
    auto* holder = new QWidget;
    auto* layout = new QVBoxLayout(holder);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    auto* heading = new QLabel(title);
    heading->setObjectName("section_" + title.toLower());
    QFont bold = heading->font();
    bold.setBold(true);
    heading->setFont(bold);
    layout->addWidget(heading);
    auto* rows = new QWidget;
    body = new QVBoxLayout(rows);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(2);
    layout->addWidget(rows);
    return holder;
}

void Clear(QVBoxLayout* layout) {
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->setParent(nullptr);
            widget->deleteLater();
        }
        delete item;
    }
}

}

CharacterDrop::CharacterDrop(QWidget* parent) : QLabel(parent) {
    setAcceptDrops(true);
}

void CharacterDrop::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasFormat(kCharacterMime)) event->acceptProposedAction();
}

void CharacterDrop::dropEvent(QDropEvent* event) {
    if (!event->mimeData()->hasFormat(kCharacterMime)) return;
    bool read = false;
    const uint32_t character = event->mimeData()->data(kCharacterMime).toUInt(&read);
    if (!read || character > std::numeric_limits<uint16_t>::max()) return;
    event->acceptProposedAction();
    emit CharacterDropped(static_cast<uint16_t>(character));
}

ScrubLabel::ScrubLabel(const QString& text, QWidget* parent) : QWidget(parent), text_(text) {
    setCursor(Qt::SizeHorCursor);
    setFixedHeight(20);
    setMinimumWidth(QFontMetrics(font()).horizontalAdvance(text) + 4);
}

void ScrubLabel::mousePressEvent(QMouseEvent* event) {
    pressed_x_ = static_cast<int>(event->position().x());
    sent_ = 0;
}

void ScrubLabel::mouseMoveEvent(QMouseEvent* event) {
    if (!pressed_x_) return;
    const int moved = (static_cast<int>(event->position().x()) - *pressed_x_) / kScrubStep;
    if (moved == sent_) return;
    emit Scrubbed(moved - sent_);
    sent_ = moved;
}

void ScrubLabel::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setPen(palette().color(QPalette::WindowText));
    painter.drawText(event->rect(), Qt::AlignLeft | Qt::AlignVCenter, text_);
}

Inspector::Inspector(QWidget* parent)
    : QWidget(parent), title_(new QLabel), detail_(new QLabel), badge_(new QLabel),
      raw_(new QTableWidget(0, 2)) {
    title_->setObjectName("inspector_title");
    detail_->setObjectName("inspector_detail");
    badge_->setObjectName("inspector_badge");
    QFont big = title_->font();
    big.setPointSizeF(big.pointSizeF() + 2);
    big.setBold(true);
    title_->setFont(big);

    raw_->setObjectName("raw_fields");
    raw_->setHorizontalHeaderLabels({tr("Field"), tr("Value")});
    raw_->verticalHeader()->setVisible(false);
    raw_->horizontalHeader()->setStretchLastSection(true);
    raw_->setSelectionBehavior(QAbstractItemView::SelectRows);

    transform_ = Section(tr("Transform"), transform_rows_);
    appearance_ = Section(tr("Appearance"), appearance_rows_);
    QVBoxLayout* content_rows = nullptr;
    content_ = Section(tr("Content"), content_rows);
    character_ = new CharacterDrop;
    character_->setObjectName("content_character");
    connect(character_, &CharacterDrop::CharacterDropped, this, &Inspector::CharacterDropped);
    auto* replace = new QToolButton;
    replace->setObjectName("content_replace");
    replace->setText(tr("Replace..."));
    connect(replace, &QToolButton::clicked, this, &Inspector::CharacterReplaceAsked);
    content_rows->addWidget(Row(character_, replace));
    blend_ = new QComboBox;
    blend_->setObjectName("content_blend");
    for (const Document::BlendMode& mode : Document::BlendModes())
        blend_->addItem(QString::fromStdString(Document::BlendName(mode.value)), mode.value);
    connect(blend_, &QComboBox::activated, this,
            [this](int row) { emit BlendChosen(blend_->itemData(row).toInt()); });
    content_rows->addWidget(Row(new QLabel(tr("Blend")), blend_));
    clip_depth_ = new QSpinBox;
    clip_depth_->setObjectName("content_clip_depth");
    clip_depth_->setRange(0, std::numeric_limits<uint16_t>::max());
    clip_depth_->setKeyboardTracking(false);
    clip_depth_->setToolTip(tr("Depths up to this one are masked by this one; 0 masks nothing"));
    connect(clip_depth_, &QSpinBox::editingFinished, this, [this] {
        if (clip_depth_->value() == shown_clip_depth_) return;
        shown_clip_depth_ = clip_depth_->value();
        emit ClipDepthEdited(shown_clip_depth_);
    });
    content_rows->addWidget(Row(new QLabel(tr("Masks up to depth")), clip_depth_));
    auto* filters = new QWidget;
    filters_ = new QVBoxLayout(filters);
    filters_->setContentsMargins(0, 0, 0, 0);
    filters_->setSpacing(2);
    content_rows->addWidget(filters);
    adding_ = new QWidget;
    auto* add_row = new QHBoxLayout(adding_);
    add_row->setContentsMargins(0, 0, 0, 0);
    add_row->setSpacing(6);
    for (const auto& [name, text, hsv] :
         {std::tuple{QStringLiteral("filter_add_matrix"), tr("Add a colour matrix"), false},
          std::tuple{QStringLiteral("filter_add_hsv"), tr("Add HSV"), true}}) {
        auto* button = new QToolButton;
        button->setObjectName(name);
        button->setText(text);
        connect(button, &QToolButton::clicked, this, [this, hsv] { emit FilterAdded(hsv); });
        add_row->addWidget(button);
    }
    add_row->addStretch();
    content_rows->addWidget(adding_);
    content_->setVisible(false);

    ease_ = new EaseEditor;
    connect(ease_, &EaseEditor::EaseChosen, this, &Inspector::EaseChosen);
    QVBoxLayout* keyed_rows = nullptr;
    keyframes_ = Section(tr("Keyframes"), keyed_rows);
    keyed_rows->addWidget(ease_);
    keyframes_->setVisible(false);

    auto* inside = new QWidget;
    auto* layout = new QVBoxLayout(inside);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    auto* heading = new QVBoxLayout;
    heading->setSpacing(1);
    heading->addWidget(title_);
    heading->addWidget(detail_);
    heading->addWidget(badge_);
    layout->addLayout(heading);
    layout->addWidget(content_);
    layout->addWidget(transform_);
    layout->addWidget(appearance_);
    layout->addWidget(keyframes_);
    layout->addStretch();
    raw_heading_ = new QToolButton;
    raw_heading_->setObjectName("section_raw");
    raw_heading_->setText(tr("Raw placement fields"));
    raw_heading_->setCheckable(true);
    raw_heading_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    raw_heading_->setArrowType(Qt::RightArrow);
    connect(raw_heading_, &QToolButton::toggled, this, [this](bool open) {
        raw_->setVisible(open);
        raw_heading_->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
    });
    raw_->setVisible(false);
    layout->addWidget(raw_heading_);
    layout->addWidget(raw_, 1);

    auto* area = new QScrollArea;
    area->setWidget(inside);
    area->setWidgetResizable(true);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(area);
}

void Inspector::ShowExtras(const std::optional<PlacementExtras>& extras) {
    content_->setVisible(extras.has_value());
    if (!extras) return;
    character_->setText(extras->character.isEmpty() ? tr("No character placed")
                                                    : extras->character);
    const int row = blend_->findData(extras->blend);
    const QSignalBlocker held(blend_);
    if (row >= 0) {
        blend_->setCurrentIndex(row);
    } else {
        blend_->addItem(
            QString::fromStdString(Document::BlendName(static_cast<uint8_t>(extras->blend))),
            extras->blend);
        blend_->setCurrentIndex(blend_->count() - 1);
    }
    const QSignalBlocker stopped(clip_depth_);
    shown_clip_depth_ = extras->clip_depth;
    clip_depth_->setValue(extras->clip_depth);
    adding_->setVisible(extras->owned);
    Clear(filters_);
    for (const QString& filter : extras->filters) {
        auto* name = new QLabel(filter);
        const QString called = filter.section(':', 0, 0);
        if (!extras->owned) {
            filters_->addWidget(name);
            continue;
        }
        auto* remove = new QToolButton;
        remove->setObjectName("filter_remove_" + called);
        remove->setText(tr("Remove"));
        connect(remove, &QToolButton::clicked, this,
                [this, called] { emit FilterRemoved(called); });
        filters_->addWidget(Row(name, remove));
    }
}

void Inspector::ShowEase(const std::optional<EaseView>& ease) {
    keyframes_->setVisible(ease.has_value());
    if (ease) ease_->Show(*ease);
}

void Inspector::ShowSubject(const InspectorSubject& subject) {
    title_->setText(subject.title);
    detail_->setText(subject.detail);
    badge_->setText(!subject.depth_chosen ? QString()
                    : subject.owned       ? tr("KEYED, edited through its keyframes")
                                          : tr("BAKED"));
}

void Inspector::ShowView(const std::optional<Document::PlacementView>& view, uint32_t frame) {
    Clear(transform_rows_);
    Clear(appearance_rows_);
    transform_->setVisible(view.has_value());
    appearance_->setVisible(view.has_value());
    if (!view) return;
    AddRows(transform_rows_, view->transform, frame);
    AddRows(appearance_rows_, view->colours, frame);
}

void Inspector::AddRows(QVBoxLayout* into, const std::vector<Document::ViewRow>& rows,
                        uint32_t frame) {
    for (const Document::ViewRow& row : rows)
        AddRow(into, row, frame);
}

void Inspector::AddRow(QVBoxLayout* into, const Document::ViewRow& row, uint32_t frame) {
    const QString label = QString::fromStdString(row.label);
    auto* left = new QWidget;
    auto* heading = new QHBoxLayout(left);
    heading->setContentsMargins(0, 0, 0, 0);
    heading->setSpacing(4);
    left->setFixedWidth(kLabelWidth + 42);

    if (row.keying != Document::Keying::Baked) {
        auto* keying = new QToolButton;
        keying->setObjectName("keying_" + label);
        const bool animated = row.keying != Document::Keying::NotAnimated;
        keying->setText(row.keying == Document::Keying::KeyedHere ? QStringLiteral("K")
                        : animated                                ? QStringLiteral("k")
                                                                  : QStringLiteral("+"));
        keying->setToolTip(row.keying == Document::Keying::KeyedHere ? tr("Keyed on this frame")
                           : animated ? tr("Animated, no keyframe on this frame")
                                      : tr("Start animating this value"));
        connect(keying, &QToolButton::clicked, this,
                [this, label, animated] { emit KeyingToggled(label, animated); });
        heading->addWidget(keying);
    }

    auto* mark = new QLabel(row.set_on == frame ? QStringLiteral("*") : QStringLiteral("."));
    mark->setObjectName("set_on_" + label);
    mark->setToolTip(row.set_on == frame ? tr("Set on this frame")
                                         : tr("Set on frame %1").arg(row.set_on));
    heading->addWidget(mark);

    auto* name = new QLabel(label);
    name->setFixedWidth(kLabelWidth);
    heading->addWidget(name);
    heading->addStretch();

    into->addWidget(
        Row(left, row.unit == Document::ViewUnit::Colour ? ColourBoxes(row) : NumberBoxes(row)));
}

QWidget* Inspector::NumberBoxes(const Document::ViewRow& row) {
    const QString label = QString::fromStdString(row.label);
    auto* holder = new QWidget;
    auto* layout = new QHBoxLayout(holder);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    const QStringList axes = AxisNames(row);
    std::vector<QDoubleSpinBox*> boxes;
    for (std::size_t i = 0; i < row.values.size(); i++) {
        auto* box = new QDoubleSpinBox;
        box->setObjectName(QString("value_%1_%2").arg(label).arg(i));
        box->setDecimals(2);
        box->setRange(-1e9, 1e9);
        box->setSuffix(UnitSuffix(row.unit));
        box->setValue(row.values[i]);
        box->setKeyboardTracking(false);
        boxes.push_back(box);
        if (!axes.at(static_cast<int>(i)).isEmpty()) {
            auto* axis = new ScrubLabel(axes.at(static_cast<int>(i)));
            connect(axis, &ScrubLabel::Scrubbed, box,
                    [box](int steps) { box->setValue(box->value() + steps); });
            layout->addWidget(axis);
        }
        layout->addWidget(box);
    }
    for (QDoubleSpinBox* box : boxes) {
        connect(box, &QDoubleSpinBox::editingFinished, this, [this, label, boxes] {
            std::vector<double> values;
            values.reserve(boxes.size());
            for (const QDoubleSpinBox* one : boxes)
                values.push_back(one->value());
            emit ValueEdited(label, values);
        });
    }
    return holder;
}

QWidget* Inspector::ColourBoxes(const Document::ViewRow& row) {
    const QString label = QString::fromStdString(row.label);
    auto* holder = new QWidget;
    auto* layout = new QHBoxLayout(holder);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto* swatch = new QToolButton;
    swatch->setObjectName("colour_" + label);
    swatch->setFixedSize(kSwatch, kSwatch);
    const QColor shown(static_cast<int>(row.values.at(0)), static_cast<int>(row.values.at(1)),
                       static_cast<int>(row.values.at(2)));
    swatch->setStyleSheet(QString("background:%1").arg(shown.name()));
    connect(swatch, &QToolButton::clicked, this, [this, label] { emit ColourPicked(label); });
    layout->addWidget(swatch);

    auto* hex = new QLineEdit(HexOf(row.values));
    hex->setObjectName("hex_" + label);
    hex->setMaxLength(6);
    layout->addWidget(hex);

    auto* alpha = new QSpinBox;
    alpha->setObjectName("alpha_" + label);
    alpha->setRange(0, static_cast<int>(kColourHighest));
    alpha->setValue(static_cast<int>(row.values.at(3)));
    alpha->setKeyboardTracking(false);
    layout->addWidget(alpha);

    const auto edited = [this, label, hex, alpha] {
        bool read = false;
        const int packed = hex->text().toInt(&read, 16);
        if (!read) return;
        emit ValueEdited(label,
                         {static_cast<double>((packed >> 16) & 0xFF),
                          static_cast<double>((packed >> 8) & 0xFF),
                          static_cast<double>(packed & 0xFF), static_cast<double>(alpha->value())});
    };
    connect(hex, &QLineEdit::editingFinished, this, edited);
    connect(alpha, &QSpinBox::editingFinished, this, edited);
    return holder;
}

}
