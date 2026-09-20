#include "editor_timeline_bar.h"

#include "editor_commands.h"
#include "editor_icons.h"
#include "editor_theme.h"

#include <QAction>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QString>
#include <QToolButton>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <utility>

namespace Editor {

namespace {

constexpr int kBarHeight = 36;
constexpr int kTransportWidth = 276;
constexpr int kButtonSide = 28;
constexpr int kPlayWidth = 34;
constexpr int kIconSide = 16;
constexpr int kFrameWidth = 56;
constexpr int kFieldHeight = 24;
constexpr int kCountWidth = 54;
constexpr int kTimeWidth = 54;
constexpr int kLabelWidth = 90;
constexpr int kWorkAreaWidth = 76;
constexpr int kClearSide = 20;
constexpr int kSmallIcon = 12;
constexpr int kZoomWidth = 90;
constexpr int kZoomButtonSide = 22;
constexpr int kLeastPixels = 1;
constexpr int kMostPixels = 48;

QLabel* Faint(const QString& name, int width) {
    auto* label = new QLabel;
    label->setObjectName(name);
    label->setFont(QFont(Theme::MonoFamily(), -1));
    label->setFixedWidth(width);
    return label;
}

}

TimelineBar::TimelineBar(Commands& commands, QWidget* parent)
    : QWidget(parent), commands_(commands), frame_(new QSpinBox), count_(new QLabel),
      time_(new QLabel), label_(new QLabel), work_area_(new QLabel), timeline_(new QToolButton),
      graph_(new QToolButton), zoom_(new QSlider) {
    setObjectName("timeline_bar");
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    setFixedHeight(kBarHeight);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 10, 0);
    layout->setSpacing(0);
    layout->addStretch();
}

void TimelineBar::Build() {
    auto* layout = qobject_cast<QHBoxLayout*>(this->layout());
    if (layout == nullptr || layout->count() > 1) return;
    delete layout->takeAt(0);

    auto* transport = new QWidget;
    transport->setObjectName("transport");
    transport->setFixedWidth(kTransportWidth);
    auto* line = new QHBoxLayout(transport);
    line->setContentsMargins(6, 0, 0, 0);
    line->setSpacing(0);
    const std::array<std::pair<QString, Icons::Glyph>, 8> keys{
        {{QStringLiteral("play.first"), Icons::Glyph::First},
         {QStringLiteral("play.previous"), Icons::Glyph::Previous},
         {QStringLiteral("play.toggle"), Icons::Glyph::Play},
         {QStringLiteral("play.next"), Icons::Glyph::Next},
         {QStringLiteral("play.last"), Icons::Glyph::Last},
         {QStringLiteral("play.previous_change"), Icons::Glyph::Key},
         {QStringLiteral("play.next_change"), Icons::Glyph::Key},
         {QStringLiteral("play.loop"), Icons::Glyph::Loop}}};
    for (const auto& [id, glyph] : keys)
        line->addWidget(AddCommand(id, glyph));
    line->addStretch();
    layout->addWidget(transport);
    layout->addSpacing(10);

    frame_->setObjectName("timeline_frame");
    frame_->setRange(0, 0);
    frame_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    frame_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    frame_->setFixedSize(kFrameWidth, kFieldHeight);
    frame_->setKeyboardTracking(false);
    connect(frame_, &QSpinBox::editingFinished, this,
            [this] { emit FrameTyped(static_cast<uint32_t>(frame_->value())); });
    layout->addWidget(frame_);
    layout->addSpacing(6);
    count_->setObjectName("timeline_count");
    count_->setFont(QFont(Theme::MonoFamily(), -1));
    count_->setFixedWidth(kCountWidth);
    layout->addWidget(count_);
    time_->setObjectName("timeline_time");
    time_->setFont(QFont(Theme::MonoFamily(), -1));
    time_->setFixedWidth(kTimeWidth);
    layout->addWidget(time_);
    layout->addSpacing(14);

    auto* marker = new QLabel;
    marker->setObjectName("timeline_marker");
    marker->setPixmap(Icons::Drawn(Icons::Glyph::Key, Theme::kAmber, kSmallIcon));
    layout->addWidget(marker);
    label_->setObjectName("timeline_label");
    label_->setContentsMargins(4, 0, 0, 0);
    label_->setFixedWidth(kLabelWidth);
    layout->addWidget(label_);
    layout->addSpacing(14);

    auto* named = new QLabel(tr("Work area"));
    named->setObjectName("timeline_work_named");
    layout->addWidget(named);
    work_area_ = Faint("timeline_work_area", kWorkAreaWidth);
    work_area_->setContentsMargins(6, 0, 0, 0);
    layout->addWidget(work_area_);
    auto* clear = new QToolButton;
    clear->setObjectName("transport_clip.work_clear");
    clear->setProperty("transport", true);
    clear->setIcon(Icons::Of(Icons::Glyph::Cross, Theme::kSoft, kSmallIcon));
    clear->setIconSize(QSize(kSmallIcon, kSmallIcon));
    clear->setFixedSize(kClearSide, kClearSide);
    clear->setToolTip(tr("Clear the work area"));
    connect(clear, &QToolButton::clicked, this,
            [this] { commands_.Run(QStringLiteral("clip.work_clear")); });
    layout->addWidget(clear);
    layout->addStretch();

    auto* modes = new QFrame;
    modes->setObjectName("timeline_modes");
    auto* mode_line = new QHBoxLayout(modes);
    mode_line->setContentsMargins(0, 0, 0, 0);
    mode_line->setSpacing(0);
    timeline_->setObjectName("timeline_mode");
    timeline_->setText(tr("Timeline"));
    timeline_->setCheckable(true);
    timeline_->setChecked(true);
    graph_->setObjectName("graph_mode");
    graph_->setText(tr("Graph"));
    graph_->setCheckable(true);
    for (QToolButton* mode : {timeline_, graph_}) {
        mode->setProperty("mode", true);
        mode->setFixedHeight(kFieldHeight);
        mode_line->addWidget(mode);
    }
    connect(timeline_, &QToolButton::clicked, this, [this] {
        timeline_->setChecked(true);
        graph_->setChecked(false);
        emit GraphAsked(false);
    });
    connect(graph_, &QToolButton::clicked, this, [this] {
        graph_->setChecked(true);
        timeline_->setChecked(false);
        emit GraphAsked(true);
    });
    layout->addWidget(modes);
    layout->addSpacing(10);

    layout->addWidget(ZoomStep("view.timeline_out", Icons::Glyph::Minus));
    zoom_->setObjectName("timeline_zoom");
    zoom_->setOrientation(Qt::Horizontal);
    zoom_->setRange(kLeastPixels, kMostPixels);
    zoom_->setFixedWidth(kZoomWidth);
    connect(zoom_, &QSlider::valueChanged, this,
            [this](int pixels) { emit ZoomAsked(static_cast<double>(pixels)); });
    layout->addWidget(zoom_);
    layout->addWidget(ZoomStep("view.timeline_in", Icons::Glyph::Plus));
}

QToolButton* TimelineBar::ZoomStep(const QString& id, Icons::Glyph glyph) {
    auto* button = new QToolButton;
    button->setObjectName("transport_" + id);
    button->setProperty("transport", true);
    button->setIcon(Icons::Of(glyph, Theme::kSoft, kSmallIcon));
    button->setIconSize(QSize(kSmallIcon, kSmallIcon));
    button->setFixedSize(kZoomButtonSide, kZoomButtonSide);
    QAction* action = commands_.Action(id);
    if (action != nullptr) button->setToolTip(action->text().remove('&'));
    connect(button, &QToolButton::clicked, this, [this, id] { commands_.Run(id); });
    return button;
}

QToolButton* TimelineBar::AddCommand(const QString& id, Icons::Glyph glyph) {
    auto* button = new QToolButton;
    button->setObjectName("transport_" + id);
    button->setProperty("transport", true);
    button->setIconSize(QSize(kIconSide, kIconSide));
    button->setFixedSize(id.endsWith("toggle") ? kPlayWidth : kButtonSide, kButtonSide);
    QAction* action = commands_.Action(id);
    if (action != nullptr)
        button->setToolTip(action->text().remove('&') + " (" +
                           action->shortcut().toString(QKeySequence::NativeText) + ")");
    if (id.endsWith("toggle")) button->setProperty("play", true);
    if (action != nullptr && action->isCheckable()) {
        button->setIcon(Icons::Toggling(glyph, Theme::kSoft, Theme::kOnChosen, kIconSide));
        button->setCheckable(true);
        button->setChecked(action->isChecked());
        connect(action, &QAction::toggled, button, &QToolButton::setChecked);
    } else {
        button->setIcon(Icons::Of(glyph, Theme::kSoft, kIconSide));
    }
    connect(button, &QToolButton::clicked, this, [this, id] { commands_.Run(id); });
    return button;
}

void TimelineBar::Show(const TimelineState& state) {
    const QSignalBlocker blocked(frame_);
    frame_->setRange(0, state.frame_count == 0 ? 0 : static_cast<int>(state.frame_count) - 1);
    frame_->setValue(static_cast<int>(state.frame));
    count_->setText(state.frame_count == 0 ? QString() : tr("/ %1").arg(state.frame_count - 1));
    time_->setText(state.rate > 0 ? tr("%1 s").arg(state.frame / state.rate, 0, 'f', 2)
                                  : QString());
    label_->setText(state.label);
    label_->parentWidget()
        ->findChild<QLabel*>("timeline_marker")
        ->setVisible(!state.label.isEmpty());
    work_area_->setText(
        state.work_area
            ? tr("%1 to %2").arg(state.work_area->first_frame).arg(state.work_area->last_frame)
            : tr("none"));
    const QSignalBlocker held(zoom_);
    const int pixels = static_cast<int>(std::lround(state.zoom_pixels));
    zoom_->setRange(kLeastPixels, std::max(kMostPixels, pixels));
    zoom_->setValue(pixels);
    const bool refused_clear = commands_.Refusal("clip.work_clear").has_value();
    if (auto* clear = findChild<QToolButton*>("transport_clip.work_clear"))
        clear->setEnabled(!refused_clear);
}

}
