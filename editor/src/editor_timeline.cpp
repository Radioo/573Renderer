#include "editor_timeline.h"

#include "editor_mime.h"

#include "document/keyframes.h"
#include "document/outline.h"
#include "document/timeline.h"
#include "document/timeline_snap.h"

#include <QColor>
#include <QMimeData>
#include <QDropEvent>
#include <QDragMoveEvent>
#include <QDragEnterEvent>
#include <QContextMenuEvent>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRectF>
#include <QPen>
#include <QBrush>
#include <QPaintEvent>
#include <QPolygon>
#include <QRect>
#include <QScrollArea>
#include <QScrollBar>
#include <QString>
#include <QToolTip>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace Editor {

namespace {

constexpr int kGutterWidth = 56;
constexpr int kRulerHeight = 26;
constexpr int kRowHeight = 16;
constexpr int kBarInset = 2;
constexpr int kShortestNamedBar = 24;
constexpr int kEyeLeft = 3;
constexpr int kLockLeft = 17;
constexpr int kSwitchWidth = 12;
constexpr int kNumberLeft = 30;
constexpr int kNamePadding = 3;
constexpr int kNamePixels = 10;
constexpr int kEmptyHeight = 80;
constexpr int kLabelReach = 30;
constexpr int kLabelGrab = 4;
constexpr int kKeyReach = 6;
constexpr int kKeyRadius = 4;
constexpr int kPropertyIndent = 8;
constexpr int kSpanDragThreshold = 4;
constexpr int kEdgeReach = 3;
constexpr double kSnapPixels = 8.0;
constexpr double kZoomStep = 1.25;
constexpr double kMostPixelsPerFrame = 48.0;
constexpr int kTickSpacing = 60;
constexpr int kTickHeight = 6;
constexpr std::array<uint32_t, 10> kTickSteps{1, 2, 5, 10, 20, 50, 100, 200, 500, 1000};

const QColor kRuler(58, 58, 62);
const QColor kRow(40, 40, 44);
const QColor kSpanName(240, 240, 244);
const QColor kSwitchOn(222, 222, 228);
const QColor kSwitchOff(96, 96, 104);
const QColor kPropertyRow(34, 34, 38);
const QColor kSelectedRow(44, 66, 88);
const QColor kBar(70, 128, 196);
const QColor kHiddenBar(92, 92, 98);
const QColor kKey(210, 210, 216);
const QColor kKeySelected(240, 190, 80);
const QColor kKeyLine(96, 96, 104);
const QColor kLabelMark(220, 180, 90);
const QColor kPlayhead(230, 90, 90);
const QColor kWorkArea(72, 96, 132);

}

Timeline::Timeline(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(kEmptyHeight);
    setFocusPolicy(Qt::ClickFocus);
    setMouseTracking(true);
    setAcceptDrops(true);
}

void Timeline::ShowAnimation(uint32_t frame_count, std::vector<Document::DepthRow> rows,
                             std::vector<Document::AnimationLabel> labels) {
    frame_count_ = frame_count;
    rows_ = std::move(rows);
    labels_ = std::move(labels);
    frame_ = 0;
    keyed_depth_.reset();
    tracks_.clear();
    selected_keys_.clear();
    Resize();
    ApplyZoom();
    update();
}

void Timeline::ShowKeys(std::optional<uint16_t> depth, std::vector<Document::Track> tracks) {
    if (keyed_depth_ != depth) selected_keys_.clear();
    keyed_depth_ = depth;
    tracks_ = std::move(tracks);
    std::erase_if(selected_keys_, [this](const Document::KeyRef& key) {
        const auto track = std::ranges::find(tracks_, key.property, &Document::Track::property);
        return track == tracks_.end() ||
               std::ranges::find(track->keys, key.frame, &Document::Keyframe::frame) ==
                   track->keys.end();
    });
    Resize();
    update();
}

void Timeline::SelectKey(const QString& property, std::optional<uint32_t> frame) {
    selected_keys_.clear();
    if (frame) selected_keys_.push_back({.property = property.toStdString(), .frame = *frame});
    update();
}

void Timeline::SelectKeys(std::vector<Document::KeyRef> keys) {
    selected_keys_ = std::move(keys);
    update();
}

bool Timeline::IsSelected(const Document::KeyRef& key) const {
    return std::ranges::find(selected_keys_, key) != selected_keys_.end();
}

void Timeline::SelectDepth(std::optional<uint16_t> depth) {
    SelectDepths(depth ? std::vector<uint16_t>{*depth} : std::vector<uint16_t>{});
}

void Timeline::SelectDepths(std::vector<uint16_t> depths) {
    if (selected_depths_ == depths) return;
    selected_depths_ = std::move(depths);
    update();
}

void Timeline::Clear() {
    selected_depths_.clear();
    selected_keys_.clear();
    frame_count_ = 0;
    frame_ = 0;
    rows_.clear();
    labels_.clear();
    keyed_depth_.reset();
    tracks_.clear();
    zoom_.reset();
    setMinimumHeight(kEmptyHeight);
    setMinimumWidth(0);
    update();
}

void Timeline::SetFrame(uint32_t frame) {
    frame_ = frame;
    if (zoom_) {
        if (auto* area = ScrollArea())
            area->ensureVisible(FrameToX(frame), area->verticalScrollBar()->value(), kTickSpacing,
                                0);
    }
    update();
}

QScrollArea* Timeline::ScrollArea() const {
    return qobject_cast<QScrollArea*>(parentWidget() != nullptr ? parentWidget()->parentWidget()
                                                                : nullptr);
}

void Timeline::Resize() {
    const int lanes = static_cast<int>(Lanes().size());
    setMinimumHeight(std::max(kEmptyHeight, kRulerHeight + lanes * kRowHeight));
}

std::vector<Timeline::Lane> Timeline::Lanes() const {
    std::vector<Lane> lanes;
    for (const Document::DepthRow& row : rows_) {
        lanes.push_back(Lane{.is_property = false, .depth = row.depth, .track = 0});
        if (!keyed_depth_ || row.depth != *keyed_depth_) continue;
        for (std::size_t i = 0; i < tracks_.size(); i++)
            lanes.push_back(Lane{.is_property = true, .depth = row.depth, .track = i});
    }
    return lanes;
}

std::optional<std::size_t> Timeline::LaneAt(int y) const {
    const int lane = (y - kRulerHeight) / kRowHeight;
    if (lane < 0 || y < kRulerHeight) return std::nullopt;
    const std::vector<Lane> lanes = Lanes();
    if (static_cast<std::size_t>(lane) >= lanes.size()) return std::nullopt;
    return static_cast<std::size_t>(lane);
}

double Timeline::PixelsPerFrame() const {
    if (frame_count_ <= 1) return 0.0;
    return static_cast<double>(width() - kGutterWidth - 1) / static_cast<double>(frame_count_ - 1);
}

void Timeline::ApplyZoom() {
    if (!zoom_ || frame_count_ <= 1) {
        setMinimumWidth(0);
        return;
    }
    setMinimumWidth(kGutterWidth + 1 +
                    static_cast<int>(std::ceil(*zoom_ * static_cast<double>(frame_count_ - 1))));
}

void Timeline::wheelEvent(QWheelEvent* event) {
    if ((event->modifiers() & Qt::ControlModifier) == 0 || frame_count_ <= 1) {
        QWidget::wheelEvent(event);
        return;
    }
    event->accept();
    QScrollArea* area = ScrollArea();
    const int cursor = static_cast<int>(event->position().x());
    const int scrolled = area != nullptr ? area->horizontalScrollBar()->value() : 0;
    const uint32_t anchor = XToFrame(cursor);
    const double factor = event->angleDelta().y() > 0 ? kZoomStep : 1.0 / kZoomStep;
    const double wanted = std::min(PixelsPerFrame() * factor, kMostPixelsPerFrame);
    const int visible = area != nullptr ? area->viewport()->width() : width();
    const double fits =
        static_cast<double>(visible - kGutterWidth - 1) / static_cast<double>(frame_count_ - 1);
    if (wanted <= fits) {
        zoom_.reset();
    } else {
        zoom_ = wanted;
    }
    ApplyZoom();
    resize(std::max(minimumWidth(), visible), height());
    if (area != nullptr)
        area->horizontalScrollBar()->setValue(FrameToX(anchor) - (cursor - scrolled));
    update();
}

void Timeline::DrawTicks(QPainter& painter) const {
    const double spacing = PixelsPerFrame();
    if (spacing <= 0.0) return;
    uint32_t step = kTickSteps.back();
    for (const uint32_t candidate : kTickSteps) {
        if (spacing * candidate >= kTickSpacing) {
            step = candidate;
            break;
        }
    }
    painter.setPen(palette().color(QPalette::Mid));
    for (uint32_t frame = 0; frame < frame_count_; frame += step) {
        const int x = FrameToX(frame);
        painter.drawLine(x, kRulerHeight - kTickHeight, x, kRulerHeight);
        painter.drawText(x + 2, kRulerHeight - kTickHeight - 2, QString::number(frame));
    }
}

int Timeline::FrameToX(uint32_t frame) const {
    if (frame_count_ <= 1) return kGutterWidth;
    const double span = static_cast<double>(width() - kGutterWidth - 1);
    const double fraction = static_cast<double>(frame) / static_cast<double>(frame_count_ - 1);
    return kGutterWidth + static_cast<int>(span * fraction);
}

uint32_t Timeline::XToFrame(int x) const {
    if (frame_count_ <= 1) return 0;
    const double span = static_cast<double>(width() - kGutterWidth - 1);
    if (span <= 0) return 0;
    const double fraction = std::clamp(static_cast<double>(x - kGutterWidth) / span, 0.0, 1.0);
    return static_cast<uint32_t>(fraction * static_cast<double>(frame_count_ - 1) + 0.5);
}

std::optional<uint32_t> Timeline::KeyNear(std::size_t track, int x) const {
    if (track >= tracks_.size()) return std::nullopt;
    std::optional<uint32_t> found;
    int best = kKeyReach;
    for (const Document::Keyframe& key : tracks_[track].keys) {
        const int distance = std::abs(FrameToX(key.frame) - x);
        if (distance > best) continue;
        best = distance;
        found = key.frame;
    }
    return found;
}

void Timeline::ChooseAt(int x, int y) {
    if (frame_count_ == 0) return;
    const std::optional<std::size_t> lane = LaneAt(y);
    if (lane) {
        const std::vector<Lane> lanes = Lanes();
        const Lane& found = lanes[*lane];
        if (!found.is_property) emit DepthChosen(found.depth);
    }
    const uint32_t frame = XToFrame(x);
    if (frame == frame_) return;
    frame_ = frame;
    update();
    emit FrameChosen(frame);
}

QString Timeline::LabelNear(int x, int reach) const {
    QString found;
    int best = reach;
    for (const Document::AnimationLabel& label : labels_) {
        const int distance = std::abs(FrameToX(label.frame) - x);
        if (distance > best) continue;
        best = distance;
        found = QString::fromStdString(label.name);
    }
    return found;
}

void Timeline::contextMenuEvent(QContextMenuEvent* event) {
    if (frame_count_ == 0) return;
    const int x = event->pos().x();
    const std::optional<std::size_t> lane = LaneAt(event->pos().y());
    if (lane) {
        const std::vector<Lane> lanes = Lanes();
        const Lane& found = lanes[*lane];
        if (found.is_property) {
            const std::optional<uint32_t> key = KeyNear(found.track, x);
            emit KeyMenuRequested(event->globalPos(),
                                  QString::fromStdString(tracks_[found.track].property),
                                  key.value_or(XToFrame(x)), key.has_value());
            return;
        }
    }
    emit MenuRequested(event->globalPos(), XToFrame(x), LabelNear(x, kLabelReach));
}

void Timeline::PressKeys(const Lane& lane, QPoint at, bool toggle) {
    const std::optional<uint32_t> key = KeyNear(lane.track, at.x());
    if (!key) {
        band_kept_ = toggle ? selected_keys_ : std::vector<Document::KeyRef>{};
        if (!toggle) selected_keys_.clear();
        band_from_ = at;
        band_to_ = at;
        update();
        return;
    }
    const Document::KeyRef pressed{.property = tracks_[lane.track].property, .frame = *key};
    if (toggle && IsSelected(pressed)) {
        std::erase(selected_keys_, pressed);
        update();
        return;
    }
    if (!toggle && !IsSelected(pressed)) selected_keys_.clear();
    if (!IsSelected(pressed)) selected_keys_.push_back(pressed);
    drag_from_ = *key;
    drag_to_ = *key;
    update();
    emit KeyChosen(QString::fromStdString(pressed.property), pressed.frame);
}

bool Timeline::PressSwitch(const Lane& lane, QPoint at) {
    if (lane.is_property || at.x() >= kNumberLeft) return false;
    if (at.x() >= kEyeLeft && at.x() < kEyeLeft + kSwitchWidth) {
        emit VisibilityToggled(lane.depth);
        return true;
    }
    if (at.x() >= kLockLeft && at.x() < kLockLeft + kSwitchWidth) {
        emit LockToggled(lane.depth);
        return true;
    }
    return false;
}

bool Timeline::PressDepthNumber(const Lane& lane, QPoint at, Qt::KeyboardModifiers modifiers) {
    if (lane.is_property || at.x() < kNumberLeft || at.x() >= kGutterWidth) return false;
    const bool adding = (modifiers & Qt::ControlModifier) != 0;
    const bool ranging = (modifiers & Qt::ShiftModifier) != 0 && !selected_depths_.empty();
    if (!adding && !ranging) {
        SelectDepths({lane.depth});
        emit DepthChosen(lane.depth);
        return true;
    }
    std::vector<uint16_t> chosen = selected_depths_;
    if (ranging) {
        const uint16_t anchor = selected_depths_.back();
        const uint16_t low = std::min(anchor, lane.depth);
        const uint16_t high = std::max(anchor, lane.depth);
        chosen = {anchor};
        for (const Lane& other : Lanes()) {
            if (other.is_property || other.depth < low || other.depth > high) continue;
            if (other.depth != anchor && other.depth != lane.depth) chosen.push_back(other.depth);
        }
        if (lane.depth != anchor) chosen.push_back(lane.depth);
    } else if (const auto found = std::ranges::find(chosen, lane.depth); found != chosen.end()) {
        chosen.erase(found);
    } else {
        chosen.push_back(lane.depth);
    }
    SelectDepths(chosen);
    emit DepthsChosen(chosen);
    return true;
}

void Timeline::DrawSwitches(QPainter& painter, uint16_t depth, int y) const {
    const bool hidden = std::ranges::find(hidden_depths_, depth) != hidden_depths_.end();
    const bool locked = std::ranges::find(locked_depths_, depth) != locked_depths_.end();
    const int middle = y + kRowHeight / 2;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF eye(kEyeLeft, middle - 3, kSwitchWidth, 6);
    painter.setPen(QPen(hidden ? kSwitchOff : kSwitchOn, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(eye);
    if (!hidden) {
        painter.setBrush(kSwitchOn);
        painter.drawEllipse(eye.center(), 2, 2);
    }
    const QRectF body(kLockLeft + 2, middle - 1, kSwitchWidth - 4, 6);
    painter.setPen(QPen(locked ? kSwitchOn : kSwitchOff, 1));
    painter.setBrush(locked ? QBrush(kSwitchOn) : QBrush(Qt::NoBrush));
    painter.drawRect(body);
    painter.setBrush(Qt::NoBrush);
    painter.drawArc(QRectF(kLockLeft + 3, middle - 6, kSwitchWidth - 6, 8), 0, 180 * 16);
    painter.restore();
}

void Timeline::SetCharacterNames(std::map<uint16_t, QString> names) {
    names_ = std::move(names);
    update();
}

QString Timeline::SpanName(const Document::DepthRow& row, const Document::Span& span) const {
    const auto shown = row.shows.find(span.first_frame);
    if (shown == row.shows.end()) return {};
    const auto name = names_.find(shown->second);
    return name == names_.end() ? QString() : name->second;
}

void Timeline::DrawSpanName(QPainter& painter, const QString& name, const QRect& bar) const {
    if (name.isEmpty() || bar.width() < kShortestNamedBar) return;
    QFont font = painter.font();
    font.setPixelSize(kNamePixels);
    painter.setFont(font);
    const QRect inside = bar.adjusted(kNamePadding, 0, -kNamePadding, 0);
    painter.setPen(kSpanName);
    painter.drawText(inside, Qt::AlignLeft | Qt::AlignVCenter,
                     painter.fontMetrics().elidedText(name, Qt::ElideRight, inside.width()));
}

QString Timeline::SpanNameAt(QPoint at) const {
    const std::optional<std::size_t> lane = LaneAt(at.y());
    if (!lane) return {};
    const Lane found = Lanes()[*lane];
    if (found.is_property) return {};
    const auto row = std::ranges::find(rows_, found.depth, &Document::DepthRow::depth);
    const std::optional<Document::Span> span = SpanAt(found.depth, at.x());
    if (row == rows_.end() || !span) return {};
    return SpanName(*row, *span);
}

bool Timeline::event(QEvent* event) {
    if (event->type() != QEvent::ToolTip) return QWidget::event(event);
    const auto* help = static_cast<QHelpEvent*>(event);
    const QString name = SpanNameAt(help->pos());
    if (name.isEmpty()) {
        QToolTip::hideText();
        event->ignore();
    } else {
        QToolTip::showText(help->globalPos(), name, this);
    }
    return true;
}

std::optional<Document::Span> Timeline::SpanAt(uint16_t depth, int x) const {
    const auto row = std::ranges::find(rows_, depth, &Document::DepthRow::depth);
    if (row == rows_.end()) return std::nullopt;
    for (const Document::Span& span : row->spans) {
        const int from = FrameToX(span.first_frame);
        const int to = std::max(from + 2, FrameToX(span.last_frame));
        if (x >= from && x <= to) return span;
    }
    return std::nullopt;
}

Timeline::SpanDrag Timeline::DragAt(const Document::Span& span, int x) const {
    const int to = std::max(FrameToX(span.first_frame) + 2, FrameToX(span.last_frame));
    if (std::abs(x - to) <= kEdgeReach) return SpanDrag::TrimEnd;
    if (std::abs(x - FrameToX(span.first_frame)) <= kEdgeReach) return SpanDrag::TrimStart;
    return SpanDrag::Move;
}

Document::Span Timeline::Dragged(const Document::Span& span, uint32_t to) const {
    const int64_t last = frame_count_ > 0 ? static_cast<int64_t>(frame_count_ - 1) : 0;
    switch (span_drag_) {
    case SpanDrag::TrimStart:
        return {.first_frame = std::min(to, span.last_frame), .last_frame = span.last_frame};
    case SpanDrag::TrimEnd:
        return {.first_frame = span.first_frame, .last_frame = std::max(to, span.first_frame)};
    case SpanDrag::Move:
        break;
    }
    const int64_t by = static_cast<int64_t>(to) - static_cast<int64_t>(span_from_);
    return {.first_frame = static_cast<uint32_t>(
                std::clamp<int64_t>(static_cast<int64_t>(span.first_frame) + by, 0, last)),
            .last_frame = static_cast<uint32_t>(
                std::clamp<int64_t>(static_cast<int64_t>(span.last_frame) + by, 0, last))};
}

uint32_t Timeline::SnappedTo(uint16_t depth, uint32_t to) const {
    if (!span_grabbed_ || frame_count_ <= 1) return to;
    std::vector<uint32_t> marks{0, frame_count_, frame_};
    for (const Document::AnimationLabel& label : labels_)
        marks.push_back(label.frame);
    const std::vector<uint32_t> targets =
        Document::SnapTargets(rows_, depth, *span_grabbed_, marks);
    const auto reach = static_cast<uint32_t>(kSnapPixels / PixelsPerFrame());
    switch (span_drag_) {
    case SpanDrag::TrimStart:
        return Document::SnapEdge(to, targets, reach);
    case SpanDrag::TrimEnd:
        return std::max<uint32_t>(Document::SnapEdge(to + 1, targets, reach), 1) - 1;
    case SpanDrag::Move:
        break;
    }
    const int64_t by = static_cast<int64_t>(to) - static_cast<int64_t>(span_from_);
    const int64_t snapped = Document::SnapShift(*span_grabbed_, by, targets, reach);
    return static_cast<uint32_t>(std::max<int64_t>(static_cast<int64_t>(span_from_) + snapped, 0));
}

void Timeline::ShowHoverCursor(QPoint at) {
    const std::optional<std::size_t> lane = LaneAt(at.y());
    std::optional<Document::Span> span;
    if (lane) {
        const Lane found = Lanes()[*lane];
        if (!found.is_property) span = SpanAt(found.depth, at.x());
    }
    const bool edge = span && DragAt(*span, at.x()) != SpanDrag::Move;
    setCursor(edge ? Qt::SizeHorCursor : Qt::ArrowCursor);
}

void Timeline::PressSpan(const Lane& lane, QPoint at) {
    span_grabbed_ = SpanAt(lane.depth, at.x());
    if (!span_grabbed_) return;
    span_drag_ = DragAt(*span_grabbed_, at.x());
    span_depth_ = lane.depth;
    span_press_x_ = at.x();
    span_from_ = XToFrame(at.x());
    span_to_ = span_from_;
}

void Timeline::SelectBand(bool adding) {
    std::vector<Document::KeyRef> chosen = adding ? band_kept_ : std::vector<Document::KeyRef>{};
    const QRect band = QRect(*band_from_, band_to_).normalized();
    int y = kRulerHeight;
    for (const Lane& lane : Lanes()) {
        const QRect row(0, y, width(), kRowHeight);
        y += kRowHeight;
        if (!lane.is_property || !band.intersects(row)) continue;
        const Document::Track& track = tracks_[lane.track];
        for (const Document::Keyframe& key : track.keys) {
            const int x = FrameToX(key.frame);
            if (x < band.left() - kKeyRadius || x > band.right() + kKeyRadius) continue;
            const Document::KeyRef found{.property = track.property, .frame = key.frame};
            if (std::ranges::find(chosen, found) == chosen.end()) chosen.push_back(found);
        }
    }
    selected_keys_ = std::move(chosen);
}

bool Timeline::PressLabel(QPoint at) {
    if (at.y() >= kRulerHeight) return false;
    const QString name = LabelNear(at.x(), kLabelGrab);
    const auto label =
        std::ranges::find(labels_, name.toStdString(), &Document::AnimationLabel::name);
    if (name.isEmpty() || label == labels_.end()) return false;
    label_grabbed_ = name;
    label_from_ = label->frame;
    label_to_ = label->frame;
    label_press_x_ = at.x();
    return true;
}

void Timeline::mousePressEvent(QMouseEvent* event) {
    drag_from_.reset();
    band_from_.reset();
    span_depth_.reset();
    span_dragging_ = false;
    label_grabbed_.reset();
    label_dragging_ = false;
    if (event->button() != Qt::LeftButton) return;
    if (PressLabel(event->pos())) return;
    const bool toggle = (event->modifiers() & Qt::ControlModifier) != 0;
    const std::optional<std::size_t> lane = LaneAt(event->pos().y());
    if (lane) {
        const std::vector<Lane> lanes = Lanes();
        if (PressSwitch(lanes[*lane], event->pos())) return;
        if (PressDepthNumber(lanes[*lane], event->pos(), event->modifiers())) return;
        if (lanes[*lane].is_property) {
            PressKeys(lanes[*lane], event->pos(), toggle);
        } else {
            PressSpan(lanes[*lane], event->pos());
        }
    }
    if (span_depth_) {
        emit DepthChosen(*span_depth_);
        return;
    }
    if (!(band_from_ && toggle)) ChooseAt(event->pos().x(), event->pos().y());
}

void Timeline::mouseMoveEvent(QMouseEvent* event) {
    if ((event->buttons() & Qt::LeftButton) == 0) {
        ShowHoverCursor(event->pos());
        return;
    }
    if (label_grabbed_) {
        if (!label_dragging_ && std::abs(event->pos().x() - label_press_x_) < kSpanDragThreshold)
            return;
        label_dragging_ = true;
        label_to_ = XToFrame(event->pos().x());
        update();
        return;
    }
    if (drag_from_) {
        drag_to_ = XToFrame(event->pos().x());
        update();
        return;
    }
    if (band_from_) {
        band_to_ = event->pos();
        SelectBand((event->modifiers() & Qt::ControlModifier) != 0);
        update();
        return;
    }
    if (span_depth_) {
        if (!span_dragging_ && std::abs(event->pos().x() - span_press_x_) < kSpanDragThreshold)
            return;
        span_dragging_ = true;
        span_to_ = XToFrame(event->pos().x());
        if ((event->modifiers() & Qt::ShiftModifier) != 0)
            span_to_ = SnappedTo(*span_depth_, span_to_);
        update();
        return;
    }
    ChooseAt(event->pos().x(), event->pos().y());
}

void Timeline::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    if (label_grabbed_) {
        const QString label = *label_grabbed_;
        const bool dragged = label_dragging_;
        label_grabbed_.reset();
        label_dragging_ = false;
        update();
        if (!dragged) {
            ChooseAt(event->pos().x(), event->pos().y());
            return;
        }
        if (label_to_ != label_from_) emit LabelMoved(label, label_to_);
        return;
    }
    const std::optional<uint32_t> from = drag_from_;
    const std::optional<uint16_t> span_depth = span_dragging_ ? span_depth_ : std::nullopt;
    const bool clicked_bar = span_depth_ && !span_dragging_;
    uint32_t to = XToFrame(event->pos().x());
    if (span_depth && (event->modifiers() & Qt::ShiftModifier) != 0)
        to = SnappedTo(*span_depth, to);
    drag_from_.reset();
    band_from_.reset();
    span_depth_.reset();
    span_dragging_ = false;
    update();
    if (clicked_bar) {
        ChooseAt(event->pos().x(), event->pos().y());
        return;
    }
    if (span_depth && span_grabbed_) {
        if (span_drag_ == SpanDrag::Move) {
            const int64_t moved = static_cast<int64_t>(to) - static_cast<int64_t>(span_from_);
            if (moved != 0) emit SpanMoved(*span_depth, span_from_, moved);
            return;
        }
        const Document::Span wanted = Dragged(*span_grabbed_, to);
        if (wanted != *span_grabbed_) {
            emit SpanTrimmed(*span_depth, span_grabbed_->first_frame, wanted.first_frame,
                             wanted.last_frame);
        }
        return;
    }
    if (!from) return;
    const int64_t by =
        static_cast<int64_t>(XToFrame(event->pos().x())) - static_cast<int64_t>(*from);
    if (by != 0) emit KeysShifted(by);
}

void Timeline::DrawSpanGhost(QPainter& painter, const Document::Span& span, int y) const {
    const Document::Span ghost = Dragged(span, span_to_);
    const int from = FrameToX(ghost.first_frame);
    const int to = FrameToX(ghost.last_frame);
    painter.setPen(kKeySelected);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(
        QRect(from, y + kBarInset, std::max(2, to - from), kRowHeight - 2 - 2 * kBarInset));
}

void Timeline::DrawKeys(QPainter& painter, const Document::Track& track, int y) const {
    const int64_t shift =
        drag_from_ ? static_cast<int64_t>(drag_to_) - static_cast<int64_t>(*drag_from_) : 0;
    const int middle = y + kRowHeight / 2;
    for (const Document::Keyframe& key : track.keys) {
        const bool selected = IsSelected({.property = track.property, .frame = key.frame});
        const int64_t shown = static_cast<int64_t>(key.frame) + (selected ? shift : 0);
        const int x = FrameToX(static_cast<uint32_t>(std::clamp<int64_t>(
            shown, 0, frame_count_ > 0 ? static_cast<int64_t>(frame_count_ - 1) : 0)));
        painter.setPen(Qt::NoPen);
        painter.setBrush(selected ? kKeySelected : kKey);
        if (key.ease == Document::Ease::Hold) {
            painter.drawRect(QRect(x - kKeyRadius + 1, middle - kKeyRadius + 1, 2 * kKeyRadius - 2,
                                   2 * kKeyRadius - 2));
        } else {
            const QPolygon diamond({QPoint(x, middle - kKeyRadius), QPoint(x + kKeyRadius, middle),
                                    QPoint(x, middle + kKeyRadius),
                                    QPoint(x - kKeyRadius, middle)});
            painter.drawPolygon(diamond);
        }
    }
    painter.setBrush(Qt::NoBrush);
}

void Timeline::SetHiddenDepths(std::vector<uint16_t> depths) {
    hidden_depths_ = std::move(depths);
    update();
}

void Timeline::SetWorkArea(std::optional<Document::WorkArea> area) {
    work_area_ = area;
    update();
}

void Timeline::SetLockedDepths(std::vector<uint16_t> depths) {
    locked_depths_ = std::move(depths);
    update();
}

void Timeline::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.fillRect(event->rect(), palette().color(QPalette::Base));
    if (frame_count_ == 0) {
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(rect(), Qt::AlignCenter, tr("No animation selected"));
        return;
    }

    painter.fillRect(QRect(0, 0, width(), kRulerHeight), kRuler);
    if (work_area_) {
        const int from = FrameToX(work_area_->first_frame);
        const int to = FrameToX(work_area_->last_frame + 1);
        painter.fillRect(QRect(from, 0, std::max(2, to - from), kRulerHeight), kWorkArea);
    }
    DrawTicks(painter);
    painter.setPen(palette().color(QPalette::BrightText));
    painter.drawText(QRect(0, 0, kGutterWidth, kRulerHeight), Qt::AlignCenter,
                     QString::number(frame_));
    for (const Document::AnimationLabel& label : labels_) {
        const bool dragged = label_dragging_ && label_grabbed_ &&
                             *label_grabbed_ == QString::fromStdString(label.name);
        const int x = FrameToX(dragged ? label_to_ : label.frame);
        painter.setPen(kLabelMark);
        painter.drawLine(x, 0, x, kRulerHeight);
        painter.drawText(x + 3, kRulerHeight - 8, QString::fromStdString(label.name));
    }

    int y = kRulerHeight;
    for (const Lane& lane : Lanes()) {
        if (lane.is_property) {
            const Document::Track& track = tracks_[lane.track];
            painter.fillRect(QRect(0, y, width(), kRowHeight - 1), kPropertyRow);
            painter.setPen(palette().color(QPalette::Text));
            painter.drawText(QRect(kPropertyIndent, y, kGutterWidth * 3, kRowHeight),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             QString::fromStdString(track.property));
            if (track.keys.size() > 1) {
                painter.setPen(kKeyLine);
                painter.drawLine(FrameToX(track.keys.front().frame), y + kRowHeight / 2,
                                 FrameToX(track.keys.back().frame), y + kRowHeight / 2);
            }
            DrawKeys(painter, track, y);
            y += kRowHeight;
            continue;
        }

        const auto row = std::ranges::find(rows_, lane.depth, &Document::DepthRow::depth);
        painter.fillRect(QRect(0, y, width(), kRowHeight - 1),
                         std::ranges::find(selected_depths_, lane.depth) != selected_depths_.end()
                             ? kSelectedRow
                             : kRow);
        DrawSwitches(painter, lane.depth, y);
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(QRect(kNumberLeft, y, kGutterWidth - 6 - kNumberLeft, kRowHeight),
                         Qt::AlignRight | Qt::AlignVCenter, QString::number(lane.depth));
        const bool hidden = std::ranges::find(hidden_depths_, lane.depth) != hidden_depths_.end();
        if (row != rows_.end()) {
            for (const Document::Span& span : row->spans) {
                const int from = FrameToX(span.first_frame);
                const int to = FrameToX(span.last_frame);
                const QRect bar(from, y + kBarInset, std::max(2, to - from),
                                kRowHeight - 1 - 2 * kBarInset);
                painter.fillRect(bar, hidden ? kHiddenBar : kBar);
                DrawSpanName(painter, SpanName(*row, span), bar);
                if (span_dragging_ && span_depth_ == lane.depth && span_grabbed_ == span)
                    DrawSpanGhost(painter, span, y);
            }
        }
        y += kRowHeight;
    }

    const int playhead = FrameToX(frame_);
    painter.setPen(kPlayhead);
    painter.drawLine(playhead, 0, playhead, height());
    if (band_from_) {
        painter.setPen(kKeySelected);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(QRect(*band_from_, band_to_).normalized());
    }
}

bool Timeline::TakesDrop(const QMimeData* data, QPointF at) const {
    return data->hasFormat(kCharacterMime) && frame_count_ > 0 && at.x() >= kGutterWidth;
}

void Timeline::dragEnterEvent(QDragEnterEvent* event) {
    if (TakesDrop(event->mimeData(), event->position())) event->acceptProposedAction();
}

void Timeline::dragMoveEvent(QDragMoveEvent* event) {
    if (TakesDrop(event->mimeData(), event->position())) event->acceptProposedAction();
}

void Timeline::dropEvent(QDropEvent* event) {
    if (!TakesDrop(event->mimeData(), event->position())) return;
    bool read = false;
    const uint32_t character = event->mimeData()->data(kCharacterMime).toUInt(&read);
    if (!read || character > std::numeric_limits<uint16_t>::max()) return;
    event->acceptProposedAction();
    const QPoint at = event->position().toPoint();
    std::optional<uint16_t> depth;
    if (const std::optional<std::size_t> lane = LaneAt(at.y())) {
        const Lane found = Lanes()[*lane];
        if (!found.is_property) depth = found.depth;
    }
    emit CharacterDropped(static_cast<uint16_t>(character), XToFrame(at.x()), depth);
}

}
