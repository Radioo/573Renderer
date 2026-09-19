#include "editor_viewport.h"

#include "editor_mime.h"

#include "document/stage_bounds.h"

#include <QColor>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QLineF>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QRect>
#include <QFont>
#include <QPaintEvent>
#include <QPen>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QResizeEvent>
#include <QSize>
#include <QSizeF>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace Editor {

namespace {

constexpr int kMinimumWidth = 320;
constexpr int kMinimumHeight = 180;
constexpr double kDragThreshold = 3.0;
constexpr double kHandleReach = 7.0;
constexpr double kHandleSize = 7.0;
constexpr double kTurnDistance = 28.0;
constexpr double kAnchorSize = 6.0;
constexpr int kOutlineWidth = 2;
const QColor kSelectedColour(80, 200, 255);
const QColor kGuideColour(255, 80, 200);
const QColor kPathColour(255, 200, 80);
constexpr double kPathDot = 1.5;
constexpr double kPathKeySize = 7.0;
constexpr double kSnapReach = 6.0;
constexpr double kNudge = 1.0;
constexpr double kShiftNudge = 10.0;
constexpr double kZoomStep = 1.25;
constexpr double kLeastZoom = 0.25;
constexpr double kMostZoom = 32.0;
constexpr int kWheelNotch = 120;
constexpr double kGhostOpacity = 0.35;
constexpr int kRulerSize = 16;
constexpr int kRulerTick = 5;
constexpr int kRulerFont = 8;
constexpr double kGuideReach = 4.0;
constexpr double kTickSpacing = 50.0;
constexpr std::array<int, 9> kTickSteps{1, 5, 10, 25, 50, 100, 250, 500, 1000};
const QColor kRulerColour(44, 44, 48);
const QColor kRulerMark(170, 170, 176);
const QColor kGuideLine(0, 200, 230);

QPointF Middle(QPointF a, QPointF b) {
    return (a + b) / 2.0;
}

}

Viewport::Viewport(QWidget* parent) : QWidget(parent) {
    setMinimumSize(kMinimumWidth, kMinimumHeight);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);
    message_ = tr("No animation selected");
}

void Viewport::keyPressEvent(QKeyEvent* event) {
    const Document::StageOutline* selected = SelectedOutline();
    const double step = (event->modifiers() & Qt::ShiftModifier) != 0 ? kShiftNudge : kNudge;
    QPointF by;
    switch (event->key()) {
    case Qt::Key_Left:
        by = QPointF(-step, 0);
        break;
    case Qt::Key_Right:
        by = QPointF(step, 0);
        break;
    case Qt::Key_Up:
        by = QPointF(0, -step);
        break;
    case Qt::Key_Down:
        by = QPointF(0, step);
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    if (selected == nullptr || gesture_ != Gesture::None) {
        QWidget::keyPressEvent(event);
        return;
    }
    emit Dragged(selected->depth, by.x(), by.y(), true);
    event->accept();
}

void Viewport::ShowFrame(const QImage& frame, QSize stage) {
    frame_ = frame;
    ghosts_.clear();
    stage_ = stage;
    message_.clear();
    update();
}

void Viewport::ShowMessage(const QString& message) {
    frame_ = QImage();
    ghosts_.clear();
    outlines_.clear();
    selected_.reset();
    message_ = message;
    update();
}

void Viewport::ShowOutlines(std::vector<Document::StageOutline> outlines,
                            std::optional<uint16_t> selected, std::vector<uint16_t> group) {
    outlines_ = std::move(outlines);
    selected_ = selected;
    group_ = std::move(group);
    update();
}

bool Viewport::InGroup(uint16_t depth) const {
    return std::ranges::find(group_, depth) != group_.end();
}

void Viewport::DrawGroup(QPainter& painter, const Document::StageOutline& primary) const {
    const Document::Point offset =
        dragging_ && gesture_ == Gesture::Move ? Moved(primary).offset : Document::Point{0, 0};
    painter.setBrush(Qt::NoBrush);
    for (const Document::StageOutline& outline : outlines_) {
        if (outline.depth == primary.depth || !InGroup(outline.depth)) continue;
        QPolygonF polygon;
        for (const Document::Point& corner : outline.corners)
            polygon << ToWidget({corner[0] + offset[0], corner[1] + offset[1]});
        painter.drawPolygon(polygon);
    }
}

void Viewport::DrawBand(QPainter& painter) const {
    if (!band_from_) return;
    painter.setPen(QPen(kSelectedColour, 1, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(ToWidget(*band_from_), ToWidget(band_to_)).normalized());
}

void Viewport::ShowPath(std::vector<Document::PathPoint> path) {
    path_ = std::move(path);
    update();
}

void Viewport::DrawPath(QPainter& painter) const {
    if (path_.empty() || stage_.isEmpty()) return;
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(kPathColour, 1));
    painter.setBrush(Qt::NoBrush);
    QPolygonF line;
    for (const Document::PathPoint& point : path_)
        line << ToWidget(point.at);
    painter.drawPolyline(line);
    for (const Document::PathPoint& point : path_) {
        if (!point.keyed) continue;
        const QPointF at = ToWidget(point.at);
        painter.drawRect(QRectF(at.x() - (kPathKeySize / 2), at.y() - (kPathKeySize / 2),
                                kPathKeySize, kPathKeySize));
    }
    painter.setBrush(kPathColour);
    for (const QPointF& at : line)
        painter.drawEllipse(at, kPathDot, kPathDot);
}

void Viewport::ShowGhosts(std::vector<QImage> ghosts) {
    ghosts_ = std::move(ghosts);
    update();
}

QSize Viewport::FittedSize(QSize available) const {
    if (stage_.isEmpty()) return available;
    const QSize fitted = stage_.scaled(available, Qt::KeepAspectRatio);
    if (zoom_ <= 1.0) return fitted;
    const QSize largest = fitted.width() > stage_.width() ? fitted : stage_;
    const QSize zoomed = (QSizeF(fitted) * zoom_).toSize();
    return zoomed.width() > largest.width() ? largest : zoomed;
}

void Viewport::FitStage() {
    zoom_ = 1.0;
    pan_ = QPointF();
    update();
    emit ZoomChanged();
}

void Viewport::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasFormat(kCharacterMime)) event->acceptProposedAction();
}

void Viewport::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasFormat(kCharacterMime)) event->acceptProposedAction();
}

void Viewport::dropEvent(QDropEvent* event) {
    if (!event->mimeData()->hasFormat(kCharacterMime)) return;
    bool read = false;
    const uint32_t character = event->mimeData()->data(kCharacterMime).toUInt(&read);
    const std::optional<QPointF> stage = ToStage(event->position());
    if (!read || character > std::numeric_limits<uint16_t>::max() || !stage) return;
    event->acceptProposedAction();
    emit CharacterDropped(static_cast<uint16_t>(character), stage->x(), stage->y());
}

void Viewport::wheelEvent(QWheelEvent* event) {
    if ((event->modifiers() & Qt::ControlModifier) == 0 || event->angleDelta().y() == 0) {
        QWidget::wheelEvent(event);
        return;
    }
    event->accept();
    const QRectF before = Target();
    if (before.isEmpty()) return;
    const double notches = static_cast<double>(event->angleDelta().y()) / kWheelNotch;
    zoom_ = std::clamp(zoom_ * std::pow(kZoomStep, notches), kLeastZoom, kMostZoom);
    const QPointF cursor = event->position();
    const QPointF fraction((cursor.x() - before.x()) / before.width(),
                           (cursor.y() - before.y()) / before.height());
    const QSizeF after = Target().size();
    const QPointF top_left(cursor.x() - (fraction.x() * after.width()),
                           cursor.y() - (fraction.y() * after.height()));
    pan_ = top_left + QPointF(after.width() / 2, after.height() / 2) -
           QPointF(width() / 2.0, height() / 2.0);
    update();
    emit ZoomChanged();
}

void Viewport::ClearGuides() {
    guides_.clear();
    dragged_guide_.reset();
    update();
}

void Viewport::SetRulers(bool on) {
    rulers_ = on;
    update();
}

std::optional<std::size_t> Viewport::GuideNear(QPointF at) const {
    for (std::size_t index = 0; index < guides_.size(); index++) {
        const Document::SnapGuide& guide = guides_[index];
        const double line =
            guide.vertical ? ToWidget({guide.at, 0}).x() : ToWidget({0, guide.at}).y();
        const double pointer = guide.vertical ? at.x() : at.y();
        if (std::abs(pointer - line) <= kGuideReach) return index;
    }
    return std::nullopt;
}

bool Viewport::PressGuide(QPointF at) {
    const std::optional<QPointF> stage = ToStage(at);
    if (rulers_ && stage && (at.x() < kRulerSize || at.y() < kRulerSize)) {
        const bool vertical = at.y() >= kRulerSize;
        guides_.push_back(
            Document::SnapGuide{.vertical = vertical, .at = vertical ? stage->x() : stage->y()});
        dragged_guide_ = guides_.size() - 1;
        return true;
    }
    dragged_guide_ = GuideNear(at);
    return dragged_guide_.has_value();
}

void Viewport::DrawGuideLines(QPainter& painter) const {
    painter.setPen(QPen(kGuideLine, 1));
    for (const Document::SnapGuide& guide : guides_) {
        if (guide.vertical) {
            const double x = ToWidget({guide.at, 0}).x();
            painter.drawLine(QPointF(x, 0), QPointF(x, height()));
        } else {
            const double y = ToWidget({0, guide.at}).y();
            painter.drawLine(QPointF(0, y), QPointF(width(), y));
        }
    }
}

void Viewport::DrawRulers(QPainter& painter) const {
    const QRectF target = Target();
    if (stage_.isEmpty() || target.isEmpty()) return;
    const double scale = target.width() / stage_.width();
    const auto step =
        std::ranges::find_if(kTickSteps, [scale](int one) { return one * scale >= kTickSpacing; });
    const int every = step == kTickSteps.end() ? kTickSteps.back() : *step;
    painter.fillRect(QRect(0, 0, width(), kRulerSize), kRulerColour);
    painter.fillRect(QRect(0, 0, kRulerSize, height()), kRulerColour);
    QFont font = painter.font();
    font.setPixelSize(kRulerFont);
    painter.setFont(font);
    painter.setPen(kRulerMark);
    const auto first = [every](double from) {
        return static_cast<int>(std::ceil(from / every)) * every;
    };
    const QPointF top_left = ToStage(QPointF(0, 0)).value_or(QPointF());
    const QPointF bottom_right = ToStage(QPointF(width(), height())).value_or(QPointF());
    for (int at = first(top_left.x()); at <= bottom_right.x(); at += every) {
        const double x = ToWidget({static_cast<double>(at), 0}).x();
        if (x < kRulerSize) continue;
        painter.drawLine(QPointF(x, kRulerSize - kRulerTick), QPointF(x, kRulerSize));
        painter.drawText(QPointF(x + 2, kRulerFont + 1), QString::number(at));
    }
    for (int at = first(top_left.y()); at <= bottom_right.y(); at += every) {
        const double y = ToWidget({0, static_cast<double>(at)}).y();
        if (y < kRulerSize) continue;
        painter.drawLine(QPointF(kRulerSize - kRulerTick, y), QPointF(kRulerSize, y));
        painter.save();
        painter.translate(kRulerFont + 1, y - 2);
        painter.rotate(-90);
        painter.drawText(QPointF(0, 0), QString::number(at));
        painter.restore();
    }
}

void Viewport::SetSnapping(bool on) {
    snapping_ = on;
}

Document::Snapped Viewport::Moved(const Document::StageOutline& outline) const {
    const Document::Point raw{pointer_[0] - grab_[0], pointer_[1] - grab_[1]};
    const QRectF target = Target();
    if (!snapping_ || snap_suspended_ || stage_.isEmpty() || target.isEmpty())
        return Document::Snapped{.offset = raw, .guides = {}};
    const double reach = kSnapReach * stage_.width() / target.width();
    std::vector<Document::StageOutline> still;
    for (const Document::StageOutline& other : outlines_) {
        if (!InGroup(other.depth)) still.push_back(other);
    }
    return Document::SnapMove(
        outline, still, guides_,
        {static_cast<double>(stage_.width()), static_cast<double>(stage_.height())}, raw, reach);
}

void Viewport::DrawGuides(QPainter& painter, const Document::StageOutline& outline) const {
    if (!dragging_ || gesture_ != Gesture::Move) return;
    painter.setPen(QPen(kGuideColour, 1));
    const QRectF target = Target();
    for (const Document::SnapGuide& guide : Moved(outline).guides) {
        if (guide.vertical) {
            const double x = ToWidget({guide.at, 0}).x();
            painter.drawLine(QPointF(x, target.top()), QPointF(x, target.bottom()));
        } else {
            const double y = ToWidget({0, guide.at}).y();
            painter.drawLine(QPointF(target.left(), y), QPointF(target.right(), y));
        }
    }
}

QRectF Viewport::Target() const {
    const QSize shape = stage_.isEmpty() ? frame_.size() : stage_;
    const QSizeF shown = QSizeF(shape.scaled(size(), Qt::KeepAspectRatio)) * zoom_;
    const QPointF centre = QPointF(width() / 2.0, height() / 2.0) + pan_;
    return {centre - QPointF(shown.width() / 2, shown.height() / 2), shown};
}

std::optional<QPointF> Viewport::ToStage(QPointF widget) const {
    const QRectF target = Target();
    if (frame_.isNull() || stage_.isEmpty() || target.isEmpty()) return std::nullopt;
    return QPointF((widget.x() - target.x()) * stage_.width() / target.width(),
                   (widget.y() - target.y()) * stage_.height() / target.height());
}

QPointF Viewport::ToWidget(const Document::Point& stage) const {
    const QRectF target = Target();
    return {target.x() + (stage[0] * target.width() / stage_.width()),
            target.y() + (stage[1] * target.height() / stage_.height())};
}

const Document::StageOutline* Viewport::SelectedOutline() const {
    if (!selected_) return nullptr;
    for (const Document::StageOutline& outline : outlines_) {
        if (outline.depth == *selected_) return &outline;
    }
    return nullptr;
}

QPointF Viewport::TurnHandle(const Document::StageOutline& outline) const {
    QPointF centre;
    for (const Document::Point& corner : outline.corners)
        centre += ToWidget(corner) / static_cast<double>(outline.corners.size());
    const QPointF top = Middle(ToWidget(outline.corners[0]), ToWidget(outline.corners[1]));
    QLineF away(centre, top);
    if (away.length() < 1.0) away = QLineF(top, top + QPointF(0, -1));
    away.setLength(away.length() + kTurnDistance);
    return away.p2();
}

Viewport::Gesture Viewport::GestureAt(const Document::StageOutline& outline, QPointF widget,
                                      Document::Point stage) const {
    for (const Document::Point& corner : outline.corners) {
        if (QLineF(ToWidget(corner), widget).length() <= kHandleReach) return Gesture::Scale;
    }
    if (QLineF(TurnHandle(outline), widget).length() <= kHandleReach) return Gesture::Turn;
    const std::vector<Document::StageOutline> only{outline};
    return Document::DepthAt(only, stage) ? Gesture::Move : Gesture::None;
}

Document::StageOutline Viewport::Preview(const Document::StageOutline& outline) const {
    if (!dragging_) return outline;
    switch (gesture_) {
    case Gesture::Move: {
        Document::StageOutline moved = outline;
        const Document::Point offset = Moved(outline).offset;
        const double dx = offset[0];
        const double dy = offset[1];
        for (Document::Point& corner : moved.corners)
            corner = {corner[0] + dx, corner[1] + dy};
        moved.anchor = {moved.anchor[0] + dx, moved.anchor[1] + dy};
        return moved;
    }
    case Gesture::Scale:
        return Document::ReshapedOutline(outline, Document::ScaleToReach(outline, grab_, pointer_));
    case Gesture::Turn:
        return Document::ReshapedOutline(outline, Document::TurnToReach(outline, grab_, pointer_));
    case Gesture::None:
        break;
    }
    return outline;
}

void Viewport::DrawSelection(QPainter& painter, const Document::StageOutline& outline) const {
    QPolygonF polygon;
    for (const Document::Point& corner : outline.corners)
        polygon << ToWidget(corner);
    painter.setBrush(Qt::NoBrush);
    painter.drawPolygon(polygon);
    const QPointF handle = TurnHandle(outline);
    painter.drawLine(Middle(polygon[0], polygon[1]), handle);
    const QPointF anchor = ToWidget(outline.anchor);
    painter.drawLine(anchor - QPointF(kAnchorSize, 0), anchor + QPointF(kAnchorSize, 0));
    painter.drawLine(anchor - QPointF(0, kAnchorSize), anchor + QPointF(0, kAnchorSize));
    painter.setBrush(kSelectedColour);
    const QPointF half(kHandleSize / 2, kHandleSize / 2);
    for (const QPointF& corner : polygon)
        painter.drawRect(QRectF(corner - half, corner + half));
    painter.drawEllipse(handle, kHandleSize / 2, kHandleSize / 2);
}

void Viewport::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.fillRect(event->rect(), QColor(0, 0, 0));
    if (frame_.isNull()) {
        painter.setPen(palette().color(QPalette::BrightText));
        painter.drawText(rect(), Qt::AlignCenter, message_);
        return;
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(Target(), frame_);
    painter.setOpacity(kGhostOpacity);
    for (const QImage& ghost : ghosts_)
        painter.drawImage(Target(), ghost);
    painter.setOpacity(1.0);
    DrawGuideLines(painter);
    const Document::StageOutline* selected = SelectedOutline();
    if (selected != nullptr && !stage_.isEmpty()) {
        DrawGuides(painter, *selected);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(kSelectedColour, kOutlineWidth));
        DrawSelection(painter, Preview(*selected));
        DrawGroup(painter, *selected);
    }
    DrawPath(painter);
    DrawBand(painter);
    if (rulers_) DrawRulers(painter);
}

void Viewport::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    emit Resized(event->size().width(), event->size().height());
}

void Viewport::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        panning_from_ = event->position();
        return;
    }
    gesture_ = Gesture::None;
    dragging_ = false;
    if (event->button() != Qt::LeftButton) return;
    const std::optional<QPointF> stage = ToStage(event->position());
    if (!stage) return;
    const Document::Point point{stage->x(), stage->y()};
    const Document::StageOutline* selected = SelectedOutline();
    Gesture gesture =
        selected != nullptr ? GestureAt(*selected, event->position(), point) : Gesture::None;
    if (gesture != Gesture::Scale && gesture != Gesture::Turn && PressGuide(event->position()))
        return;
    if (gesture != Gesture::Scale && gesture != Gesture::Turn) {
        emit Picked(point[0], point[1]);
        selected = SelectedOutline();
        gesture =
            selected != nullptr && GestureAt(*selected, event->position(), point) == Gesture::Move
                ? Gesture::Move
                : Gesture::None;
    }
    gesture_ = gesture;
    press_ = event->position();
    grab_ = point;
    pointer_ = point;
    band_from_.reset();
    if (gesture == Gesture::None) {
        band_from_ = point;
        band_to_ = point;
    }
}

void Viewport::mouseMoveEvent(QMouseEvent* event) {
    if (panning_from_ && (event->buttons() & Qt::MiddleButton) != 0) {
        pan_ += event->position() - *panning_from_;
        panning_from_ = event->position();
        update();
        return;
    }
    if (dragged_guide_ && (event->buttons() & Qt::LeftButton) != 0) {
        const std::optional<QPointF> stage = ToStage(event->position());
        Document::SnapGuide& guide = guides_[*dragged_guide_];
        if (stage) guide.at = guide.vertical ? stage->x() : stage->y();
        update();
        return;
    }
    if (band_from_ && (event->buttons() & Qt::LeftButton) != 0) {
        const std::optional<QPointF> stage = ToStage(event->position());
        if (stage) band_to_ = {stage->x(), stage->y()};
        update();
        return;
    }
    if (gesture_ == Gesture::None || (event->buttons() & Qt::LeftButton) == 0) return;
    if (!dragging_ && QLineF(press_, event->position()).length() < kDragThreshold) return;
    const std::optional<QPointF> stage = ToStage(event->position());
    if (!stage) return;
    dragging_ = true;
    snap_suspended_ = (event->modifiers() & Qt::AltModifier) != 0;
    pointer_ = {stage->x(), stage->y()};
    update();
    const Document::StageOutline* selected = SelectedOutline();
    if (selected != nullptr) EmitGesture(gesture_, *selected, false);
}

void Viewport::EmitGesture(Gesture gesture, const Document::StageOutline& outline, bool finished) {
    switch (gesture) {
    case Gesture::Move: {
        const Document::Point offset = Moved(outline).offset;
        emit Dragged(outline.depth, offset[0], offset[1], finished);
        break;
    }
    case Gesture::Scale: {
        const Document::Reshape reshape = Document::ScaleToReach(outline, grab_, pointer_);
        emit Reshaped(outline.depth, reshape.scale_x, reshape.scale_y, reshape.turn, finished);
        break;
    }
    case Gesture::Turn: {
        const Document::Reshape reshape = Document::TurnToReach(outline, grab_, pointer_);
        emit Reshaped(outline.depth, reshape.scale_x, reshape.scale_y, reshape.turn, finished);
        break;
    }
    case Gesture::None:
        break;
    }
}

void Viewport::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        panning_from_.reset();
        return;
    }
    if (event->button() != Qt::LeftButton) return;
    if (dragged_guide_) {
        const Document::SnapGuide& guide = guides_[*dragged_guide_];
        const bool back_on_ruler = guide.vertical ? event->position().x() < kRulerSize
                                                  : event->position().y() < kRulerSize;
        if (back_on_ruler)
            guides_.erase(guides_.begin() + static_cast<std::ptrdiff_t>(*dragged_guide_));
        dragged_guide_.reset();
        update();
        return;
    }
    if (band_from_) {
        const Document::Point from = *band_from_;
        band_from_.reset();
        update();
        if (QLineF(press_, event->position()).length() < kDragThreshold) return;
        emit DepthsBanded(Document::DepthsTouching(
            outlines_, Document::Box{.left = std::min(from[0], band_to_[0]),
                                     .right = std::max(from[0], band_to_[0]),
                                     .top = std::min(from[1], band_to_[1]),
                                     .bottom = std::max(from[1], band_to_[1])}));
        return;
    }
    const Gesture gesture = dragging_ ? gesture_ : Gesture::None;
    const Document::StageOutline* selected = SelectedOutline();
    gesture_ = Gesture::None;
    dragging_ = false;
    update();
    if (selected == nullptr) return;
    const Document::StageOutline outline = *selected;
    EmitGesture(gesture, outline, true);
}

}
