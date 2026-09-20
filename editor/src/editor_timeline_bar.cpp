#include "editor_timeline_bar.h"

#include "editor_commands.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLayoutItem>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QSizePolicy>
#include <QString>
#include <QToolButton>

#include <algorithm>
#include <cmath>
#include <optional>

namespace Editor {

namespace {

constexpr int kFrameWidth = 64;
constexpr int kCountWidth = 54;
constexpr int kTimeWidth = 60;
constexpr int kLabelWidth = 90;
constexpr int kWorkAreaWidth = 150;
constexpr int kZoomWidth = 80;
constexpr int kLeastPixels = 1;
constexpr int kMostPixels = 48;

}

TimelineBar::TimelineBar(Commands& commands, QWidget* parent)
    : QWidget(parent), commands_(commands), frame_(new QSpinBox), count_(new QLabel),
      time_(new QLabel), label_(new QLabel), work_area_(new QLabel), timeline_(new QToolButton),
      graph_(new QToolButton), zoom_(new QSlider) {
    setObjectName("timeline_bar");
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(3);
    layout->addStretch();
}

void TimelineBar::Build() {
    auto* layout = qobject_cast<QHBoxLayout*>(this->layout());
    if (layout == nullptr || layout->count() > 1) return;
    delete layout->takeAt(0);

    for (const auto& [id, text] : {std::pair{QStringLiteral("play.first"), tr("|<")},
                                   std::pair{QStringLiteral("play.previous"), tr("<")},
                                   std::pair{QStringLiteral("play.toggle"), tr("Play")},
                                   std::pair{QStringLiteral("play.next"), tr(">")},
                                   std::pair{QStringLiteral("play.last"), tr(">|")},
                                   std::pair{QStringLiteral("play.previous_change"), tr("<K")},
                                   std::pair{QStringLiteral("play.next_change"), tr("K>")},
                                   std::pair{QStringLiteral("play.loop"), tr("Loop")}})
        layout->addWidget(AddCommand(id, text));

    frame_->setObjectName("timeline_frame");
    frame_->setRange(0, 0);
    frame_->setFixedWidth(kFrameWidth);
    frame_->setKeyboardTracking(false);
    connect(frame_, &QSpinBox::editingFinished, this,
            [this] { emit FrameTyped(static_cast<uint32_t>(frame_->value())); });
    layout->addWidget(frame_);
    count_->setObjectName("timeline_count");
    count_->setFixedWidth(kCountWidth);
    layout->addWidget(count_);
    time_->setObjectName("timeline_time");
    time_->setFixedWidth(kTimeWidth);
    layout->addWidget(time_);
    label_->setObjectName("timeline_label");
    label_->setFixedWidth(kLabelWidth);
    layout->addWidget(label_);
    work_area_->setObjectName("timeline_work_area");
    work_area_->setFixedWidth(kWorkAreaWidth);
    layout->addWidget(work_area_);
    layout->addWidget(AddCommand("clip.work_start", tr("Start here")));
    layout->addWidget(AddCommand("clip.work_end", tr("End here")));
    layout->addWidget(AddCommand("clip.work_clear", tr("Clear")));
    layout->addWidget(AddCommand("depth.add", tr("+ Depth")));
    layout->addStretch();

    timeline_->setObjectName("timeline_mode");
    timeline_->setText(tr("Timeline"));
    timeline_->setCheckable(true);
    timeline_->setChecked(true);
    graph_->setObjectName("graph_mode");
    graph_->setText(tr("Graph"));
    graph_->setCheckable(true);
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
    layout->addWidget(timeline_);
    layout->addWidget(graph_);
    layout->addWidget(AddCommand("view.timeline_out", tr("-")));
    zoom_->setObjectName("timeline_zoom");
    zoom_->setOrientation(Qt::Horizontal);
    zoom_->setRange(kLeastPixels, kMostPixels);
    zoom_->setFixedWidth(kZoomWidth);
    connect(zoom_, &QSlider::valueChanged, this,
            [this](int pixels) { emit ZoomAsked(static_cast<double>(pixels)); });
    layout->addWidget(zoom_);
    layout->addWidget(AddCommand("view.timeline_in", tr("+")));
}

QToolButton* TimelineBar::AddCommand(const QString& id, const QString& text) {
    auto* button = new QToolButton;
    button->setObjectName("transport_" + id);
    button->setText(text);
    QAction* action = commands_.Action(id);
    if (action != nullptr && action->isCheckable()) {
        button->setCheckable(true);
        button->setChecked(action->isChecked());
        connect(action, &QAction::toggled, button, &QToolButton::setChecked);
        connect(button, &QToolButton::clicked, this, [this, id] { commands_.Run(id); });
        return button;
    }
    connect(button, &QToolButton::clicked, this, [this, id] { commands_.Run(id); });
    return button;
}

void TimelineBar::Show(const TimelineState& state) {
    const QSignalBlocker blocked(frame_);
    frame_->setRange(0, state.frame_count == 0 ? 0 : static_cast<int>(state.frame_count) - 1);
    frame_->setValue(static_cast<int>(state.frame));
    count_->setText(state.frame_count == 0 ? QString() : tr("of %1").arg(state.frame_count - 1));
    time_->setText(state.rate > 0 ? tr("%1 s").arg(state.frame / state.rate, 0, 'f', 2)
                                  : QString());
    label_->setText(state.label);
    work_area_->setText(state.work_area ? tr("Work area %1 to %2")
                                              .arg(state.work_area->first_frame)
                                              .arg(state.work_area->last_frame)
                                        : tr("No work area"));
    const QSignalBlocker held(zoom_);
    const int pixels = static_cast<int>(std::lround(state.zoom_pixels));
    zoom_->setRange(kLeastPixels, std::max(kMostPixels, pixels));
    zoom_->setValue(pixels);
    const bool refused_clear = commands_.Refusal("clip.work_clear").has_value();
    if (auto* clear = findChild<QToolButton*>("transport_clip.work_clear"))
        clear->setEnabled(!refused_clear);
}

}
