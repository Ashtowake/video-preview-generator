#include "TimelineWidget.hpp"

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>

#include <algorithm>
#include <array>

namespace {

constexpr int kHandleHitRadiusPx = 10;
constexpr int kHandleWidthPx = 10;
constexpr int kSamplingMarkerHitRadiusPx = 9;

QRectF fitRectWithAspect(const QRectF& bounds, double aspectRatio)
{
  const double safeAspect = std::max(0.05, aspectRatio);
  const double maxWidth = std::max(1.0, bounds.width());
  const double maxHeight = std::max(1.0, bounds.height());

  double width = maxWidth;
  double height = width / safeAspect;
  if (height > maxHeight) {
    height = maxHeight;
    width = height * safeAspect;
  }

  return QRectF(
    bounds.center().x() - width / 2.0,
    bounds.center().y() - height / 2.0,
    width,
    height);
}

} // namespace

TimelineWidget::TimelineWidget(QWidget* parent)
  : QWidget(parent)
{
  setMinimumHeight(196);
  setMouseTracking(true);
  loadingTimer_.setInterval(80);
  connect(&loadingTimer_, &QTimer::timeout, this, [this] {
    loadingPhase_ = (loadingPhase_ + 1) % 24;
    update();
  });
}

void TimelineWidget::setDurationMs(qint64 durationMs)
{
  durationMs_ = std::max<qint64>(0, durationMs);
  positionMs_ = clampedPosition(positionMs_);
  dragPositionMs_ = clampedPosition(dragPositionMs_);
  rangeStartMs_ = clampedPosition(rangeStartMs_);
  rangeEndMs_ = std::clamp<qint64>(rangeEndMs_, rangeStartMs_, durationMs_);
  samplingStartMs_ = std::clamp<qint64>(samplingStartMs_, rangeStartMs_, rangeEndMs_);
  hoverPositionMs_ = clampedPosition(hoverPositionMs_);
  if (durationMs_ > 0 && rangeEndMs_ == 0) {
    rangeEndMs_ = durationMs_;
  }
  update();
}

void TimelineWidget::setPositionMs(qint64 positionMs)
{
  positionMs_ = clampedPosition(positionMs);
  if (!dragging_) {
    dragPositionMs_ = positionMs_;
    update();
    return;
  }

  if (dragMode_ != DragMode::Scrub) {
    update();
  }
}

void TimelineWidget::setSelectionRangeMs(qint64 startMs, qint64 endMs)
{
  if (durationMs_ <= 0) {
    rangeStartMs_ = 0;
    rangeEndMs_ = 0;
    samplingStartMs_ = 0;
    update();
    return;
  }

  rangeStartMs_ = clampedPosition(startMs);
  rangeEndMs_ = std::clamp<qint64>(endMs, rangeStartMs_, durationMs_);
  samplingStartMs_ = std::clamp<qint64>(samplingStartMs_, rangeStartMs_, rangeEndMs_);
  update();
}

void TimelineWidget::setSamplingStartMs(qint64 samplingStartMs)
{
  samplingStartMs_ = std::clamp<qint64>(samplingStartMs, rangeStartMs_, rangeEndMs_);
  update();
}

void TimelineWidget::setFramesPerSecond(double fps)
{
  fps_ = fps;
  update();
}

void TimelineWidget::setFilmstrip(const QPixmap& filmstrip, int slotCount, double frameAspectRatio)
{
  filmstrip_ = filmstrip;
  filmstripSlotCount_ = std::max(0, slotCount);
  filmstripFrameAspectRatio_ = std::max(0.05, frameAspectRatio);
  update();
}

void TimelineWidget::setFilmstripLoading(bool loading)
{
  filmstripLoading_ = loading;
  if (loading) {
    if (!loadingTimer_.isActive()) {
      loadingTimer_.start();
    }
  } else {
    loadingTimer_.stop();
    loadingPhase_ = 0;
  }
  update();
}

qint64 TimelineWidget::durationMs() const
{
  return durationMs_;
}

qint64 TimelineWidget::positionMs() const
{
  return dragMode_ == DragMode::Scrub ? dragPositionMs_ : positionMs_;
}

qint64 TimelineWidget::selectionRangeStartMs() const
{
  return rangeStartMs_;
}

qint64 TimelineWidget::selectionRangeEndMs() const
{
  return rangeEndMs_;
}

qint64 TimelineWidget::samplingStartMs() const
{
  return samplingStartMs_;
}

bool TimelineWidget::isScrubbing() const
{
  return dragging_ && dragMode_ == DragMode::Scrub;
}

QSize TimelineWidget::filmstripImageSize() const
{
  return filmstripRect().size().toSize();
}

void TimelineWidget::paintEvent(QPaintEvent* event)
{
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const QRectF frame = rect().adjusted(1, 1, -1, -1);
  painter.setPen(QColor("#273647"));
  painter.setBrush(QColor("#141d26"));
  painter.drawRoundedRect(frame, 10, 10);

  const QRectF filmstrip = filmstripRect();
  const QRectF groove = grooveRect();
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor("#0b1016"));
  painter.drawRoundedRect(filmstrip, 7, 7);
  painter.drawRoundedRect(groove, 7, 7);

  if (durationMs_ <= 0) {
    painter.setPen(QColor("#8ea1b3"));
    painter.drawText(frame.adjusted(16, 16, -16, -16), Qt::AlignCenter, "Timeline becomes active after loading a video.");
    return;
  }

  if (!filmstrip_.isNull()) {
    painter.save();
    painter.setClipRect(filmstrip);
    const int availableSlots = std::max(1, filmstripSlotCount_);
    const double preferredThumbWidth = std::max(36.0, (filmstrip.height() - 4.0) * filmstripFrameAspectRatio_);
    const int visibleSlots = std::clamp(
      static_cast<int>(std::floor(filmstrip.width() / preferredThumbWidth)),
      1,
      availableSlots);
    const int sourceSlotWidth = std::max(1, filmstrip_.width() / availableSlots);
    const double destinationThumbWidth = std::min(
      preferredThumbWidth,
      filmstrip.width() / static_cast<double>(visibleSlots));
    const double totalThumbWidth = destinationThumbWidth * visibleSlots;
    const double gapWidth = std::max(0.0, (filmstrip.width() - totalThumbWidth) / (visibleSlots + 1));

    for (int visibleIndex = 0; visibleIndex < visibleSlots; ++visibleIndex) {
      const int sourceIndex = visibleSlots == 1
        ? availableSlots / 2
        : static_cast<int>(std::llround(
            (static_cast<double>(visibleIndex) * static_cast<double>(availableSlots - 1))
            / static_cast<double>(visibleSlots - 1)));
      const QRect sourceSlotRect(
        sourceIndex * sourceSlotWidth,
        0,
        sourceSlotWidth,
        filmstrip_.height());
      const QRectF destinationSlotRect(
        filmstrip.left() + gapWidth + visibleIndex * (destinationThumbWidth + gapWidth),
        filmstrip.top(),
        destinationThumbWidth,
        filmstrip.height());
      const QRectF destinationContentRect = fitRectWithAspect(
        destinationSlotRect.adjusted(2.0, 2.0, -2.0, -2.0),
        filmstripFrameAspectRatio_);
      painter.drawPixmap(destinationContentRect, filmstrip_, QRectF(sourceSlotRect));
    }
    painter.restore();
  } else {
    painter.setPen(QColor("#8ea1b3"));
    painter.drawText(
      filmstrip.adjusted(0, 0, 0, -18),
      Qt::AlignCenter,
      filmstripLoading_ ? "Generating timeline thumbnails..." : "Timeline thumbnails unavailable.");
  }

  if (filmstripLoading_) {
    painter.save();
    painter.setClipRect(filmstrip);
    const double preferredThumbWidth = std::max(40.0, (filmstrip.height() - 8.0) * filmstripFrameAspectRatio_);
    const int placeholderCount = std::clamp(static_cast<int>(std::floor(filmstrip.width() / preferredThumbWidth)), 6, 18);
    const double placeholderWidth = std::min(
      preferredThumbWidth,
      filmstrip.width() / static_cast<double>(placeholderCount));
    const double totalWidth = placeholderWidth * placeholderCount;
    const double gapWidth = std::max(0.0, (filmstrip.width() - totalWidth) / (placeholderCount + 1));

    for (int index = 0; index < placeholderCount; ++index) {
      const QRectF slotRect(
        filmstrip.left() + gapWidth + index * (placeholderWidth + gapWidth),
        filmstrip.top() + 4.0,
        placeholderWidth,
        filmstrip.height() - 8.0);
      const int highlightDistance = std::abs(index - loadingPhase_);
      const int wrappedDistance = std::min(highlightDistance, 24 - highlightDistance);
      const int alpha = std::clamp(155 - wrappedDistance * 18, 42, 155);
      painter.setPen(Qt::NoPen);
      painter.setBrush(QColor(57, 82, 107, alpha));
      painter.drawRoundedRect(slotRect, 6, 6);
    }
    painter.restore();
    painter.setPen(QColor("#d6e0ea"));
    painter.drawText(
      filmstrip.adjusted(0, 0, 0, -18),
      Qt::AlignCenter,
      "Generating timeline thumbnails...");
  }

  const qint64 currentPositionMs = positionMs();
  const int rangeStartX = xForPosition(rangeStartMs_);
  const int rangeEndX = xForPosition(rangeEndMs_);
  const int samplingStartX = xForPosition(samplingStartMs_);

  painter.save();
  painter.setClipRect(filmstrip);
  painter.setPen(Qt::NoPen);
  if (rangeStartX > filmstrip.left()) {
    painter.setBrush(QColor(11, 16, 22, 150));
    painter.drawRect(QRectF(filmstrip.left(), filmstrip.top(), rangeStartX - filmstrip.left(), filmstrip.height()));
  }
  if (rangeEndX < filmstrip.right()) {
    painter.setBrush(QColor(11, 16, 22, 150));
    painter.drawRect(QRectF(rangeEndX, filmstrip.top(), filmstrip.right() - rangeEndX, filmstrip.height()));
  }
  painter.restore();

  painter.setClipRect(groove);
  const QRectF selectedRangeRect(
    rangeStartX,
    groove.top(),
    std::max(0, rangeEndX - rangeStartX),
    groove.height());
  painter.setBrush(QColor("#1b2d3f"));
  painter.drawRoundedRect(selectedRangeRect, 7, 7);

  const QRectF playedRect(groove.left(), groove.top(), std::max(0, xForPosition(currentPositionMs) - static_cast<int>(groove.left())), groove.height());
  painter.setBrush(QColor("#204b7a"));
  painter.drawRoundedRect(playedRect, 7, 7);
  painter.setClipping(false);

  painter.setPen(QColor("#31475d"));
  const qint64 tickStepMs = majorTickStepMs();
  if (tickStepMs > 0) {
    for (qint64 tickMs = 0; tickMs <= durationMs_; tickMs += tickStepMs) {
      const int x = xForPosition(tickMs);
      painter.drawLine(QPointF(x, groove.top() - 7), QPointF(x, groove.bottom() + 7));
      painter.drawText(QRectF(x - 32, groove.bottom() + 8, 64, 16), Qt::AlignCenter, formatTime(tickMs));
    }
  }

  const int playheadX = xForPosition(currentPositionMs);
  painter.setPen(QPen(QColor("#f0d46b"), 2));
  painter.drawLine(QPointF(playheadX, filmstrip.top() - 4), QPointF(playheadX, groove.bottom() + 10));
  painter.setBrush(QColor("#f0d46b"));
  painter.setPen(Qt::NoPen);
  painter.drawEllipse(QPointF(playheadX, groove.center().y()), 6, 6);

  painter.setBrush(QColor("#8bbdf0"));
  painter.drawRoundedRect(
    QRectF(rangeStartX - kHandleWidthPx / 2.0, filmstrip.top(), kHandleWidthPx, groove.bottom() - filmstrip.top() + 8.0),
    4,
    4);
  painter.drawRoundedRect(
    QRectF(rangeEndX - kHandleWidthPx / 2.0, filmstrip.top(), kHandleWidthPx, groove.bottom() - filmstrip.top() + 8.0),
    4,
    4);

  painter.setPen(QPen(QColor("#7ae0a1"), 2));
  painter.drawLine(QPointF(samplingStartX, filmstrip.top() - 4), QPointF(samplingStartX, groove.bottom() + 6));
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor("#7ae0a1"));
  painter.drawEllipse(QPointF(samplingStartX, filmstrip.top() - 6), 5, 5);

  const bool showHoverPreview = hovering_ && !dragging_ && !filmstrip_.isNull() && filmstripSlotCount_ > 0;
  if (!showHoverPreview) {
    painter.setPen(QColor("#e8edf3"));
    painter.drawText(QRectF(16, 10, width() - 32, 16), Qt::AlignLeft | Qt::AlignVCenter, formatTime(currentPositionMs));

    QString timelineHint = QStringLiteral("Range %1 - %2")
      .arg(formatTime(rangeStartMs_))
      .arg(formatTime(rangeEndMs_));
    if (fps_ > 0.0) {
      timelineHint.append(QStringLiteral("  |  %1 fps").arg(fps_, 0, 'f', 2));
    }
    painter.drawText(QRectF(16, 10, width() - 32, 16), Qt::AlignRight | Qt::AlignVCenter, timelineHint);
  }

  if (hovering_ && !dragging_) {
    const int hoverX = xForPosition(hoverPositionMs_);
    painter.setPen(QPen(QColor("#8bbdf0"), 1));
    painter.drawLine(QPointF(hoverX, filmstrip.top() - 2), QPointF(hoverX, groove.bottom() + 4));
  }

  if (showHoverPreview) {
    const double popupWidth = 168.0;
    const double popupHeight = 96.0;
    const double popupMargin = 16.0;
    const int hoverX = xForPosition(hoverPositionMs_);
    const int sourceIndex = filmstripSlotCount_ == 1
      ? 0
      : static_cast<int>(std::llround(
          (static_cast<double>(clampedPosition(hoverPositionMs_)) * static_cast<double>(filmstripSlotCount_ - 1))
          / static_cast<double>(std::max<qint64>(1, durationMs_))));
    const QRect sourceRect = sourceSlotRect(sourceIndex);
    if (!sourceRect.isEmpty()) {
      const double popupLeft = std::clamp(
        static_cast<double>(hoverX) - popupWidth / 2.0,
        popupMargin,
        std::max(popupMargin, width() - popupMargin - popupWidth));
      const QRectF popupBounds(popupLeft, 6.0, popupWidth, popupHeight);
      const QRectF previewRect = fitRectWithAspect(
        popupBounds.adjusted(8.0, 8.0, -8.0, -24.0),
        filmstripFrameAspectRatio_);
      painter.setPen(QColor("#3a5066"));
      painter.setBrush(QColor(16, 22, 29, 244));
      painter.drawRoundedRect(popupBounds, 8, 8);
      painter.drawPixmap(previewRect, filmstrip_, QRectF(sourceRect));
      painter.setPen(QColor("#f4f7fb"));
      painter.drawText(
        popupBounds.adjusted(10, 74, -10, -8),
        Qt::AlignLeft | Qt::AlignVCenter,
        formatTime(hoverPositionMs_));
      painter.setPen(QColor("#aab8c4"));
      painter.drawText(
        popupBounds.adjusted(10, 74, -10, -8),
        Qt::AlignRight | Qt::AlignVCenter,
        QStringLiteral("Sample %1").arg(sourceIndex + 1));
    }
  }
}

bool TimelineWidget::eventFilter(QObject* watched, QEvent* event)
{
  if (!dragging_ || watched == this) {
    return QWidget::eventFilter(watched, event);
  }

  switch (event->type()) {
  case QEvent::MouseMove: {
    auto* mouseEvent = static_cast<QMouseEvent*>(event);
    updateDrag(mapFromGlobal(mouseEvent->globalPosition().toPoint()), false);
    return false;
  }
  case QEvent::MouseButtonRelease: {
    auto* mouseEvent = static_cast<QMouseEvent*>(event);
    if (mouseEvent->button() == Qt::LeftButton) {
      endDrag(mapFromGlobal(mouseEvent->globalPosition().toPoint()));
    }
    return false;
  }
  default:
    return QWidget::eventFilter(watched, event);
  }
}

void TimelineWidget::mousePressEvent(QMouseEvent* event)
{
  if (event->button() != Qt::LeftButton || durationMs_ <= 0) {
    QWidget::mousePressEvent(event);
    return;
  }

  beginDrag(event->position().toPoint(), dragModeForPosition(event->position().toPoint()));
  event->accept();
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* event)
{
  hoverPositionMs_ = positionForX(event->position().toPoint().x());
  hovering_ = rect().contains(event->position().toPoint());

  if (dragging_) {
    updateDrag(event->position().toPoint(), false);
    event->accept();
    return;
  }

  update();
  QWidget::mouseMoveEvent(event);
}

void TimelineWidget::mouseReleaseEvent(QMouseEvent* event)
{
  if (!dragging_ || event->button() != Qt::LeftButton) {
    QWidget::mouseReleaseEvent(event);
    return;
  }

  endDrag(event->position().toPoint());
  event->accept();
}

void TimelineWidget::leaveEvent(QEvent* event)
{
  hovering_ = false;
  update();
  QWidget::leaveEvent(event);
}

void TimelineWidget::beginDrag(const QPoint& position, DragMode mode)
{
  dragging_ = true;
  dragMode_ = mode;
  qApp->installEventFilter(this);
  pendingPreviewMs_ = -1;
  lastDispatchedPreviewMs_ = -1;
  updateDrag(position, true);
}

void TimelineWidget::endDrag(const QPoint& position)
{
  updateDrag(position, true);
  dragging_ = false;
  qApp->removeEventFilter(this);
  pendingPreviewMs_ = -1;
  lastDispatchedPreviewMs_ = -1;
  if (dragMode_ == DragMode::Scrub) {
    emit scrubFinished(dragPositionMs_);
  } else if (dragMode_ == DragMode::SamplingStart) {
    emit samplingStartChangeFinished(samplingStartMs_);
  } else {
    emit rangeChangeFinished(rangeStartMs_, rangeEndMs_);
  }
  dragMode_ = DragMode::None;
  update();
}

void TimelineWidget::updateDrag(const QPoint& position, bool forceDispatch)
{
  const qint64 targetPositionMs = positionForX(position.x());

  if (dragMode_ == DragMode::RangeStart) {
    rangeStartMs_ = std::min(targetPositionMs, rangeEndMs_);
    update();
    emit rangePreviewChanged(rangeStartMs_, rangeEndMs_);
    return;
  }

  if (dragMode_ == DragMode::RangeEnd) {
    rangeEndMs_ = std::max(targetPositionMs, rangeStartMs_);
    update();
    emit rangePreviewChanged(rangeStartMs_, rangeEndMs_);
    return;
  }

  if (dragMode_ == DragMode::SamplingStart) {
    samplingStartMs_ = std::clamp<qint64>(targetPositionMs, rangeStartMs_, rangeEndMs_);
    update();
    emit samplingStartPreviewChanged(samplingStartMs_);
    return;
  }

  dragPositionMs_ = targetPositionMs;
  update();

  if (forceDispatch) {
    pendingPreviewMs_ = dragPositionMs_;
    dispatchPendingPreview();
    return;
  }

  pendingPreviewMs_ = dragPositionMs_;
  if (forceDispatch || pendingPreviewMs_ != lastDispatchedPreviewMs_) {
    dispatchPendingPreview();
  }
}

void TimelineWidget::dispatchPendingPreview()
{
  if (pendingPreviewMs_ < 0 || pendingPreviewMs_ == lastDispatchedPreviewMs_) {
    return;
  }

  lastDispatchedPreviewMs_ = pendingPreviewMs_;
  emit scrubPreviewRequested(pendingPreviewMs_);
}

QRectF TimelineWidget::grooveRect() const
{
  return QRectF(16.0, 130.0, std::max(32, width() - 32), 18.0);
}

QRectF TimelineWidget::filmstripRect() const
{
  return QRectF(16.0, 32.0, std::max(32, width() - 32), 84.0);
}

QRect TimelineWidget::sourceSlotRect(int sourceIndex) const
{
  if (filmstrip_.isNull() || filmstripSlotCount_ <= 0) {
    return {};
  }

  const int clampedIndex = std::clamp(sourceIndex, 0, filmstripSlotCount_ - 1);
  const int slotWidth = std::max(1, filmstrip_.width() / filmstripSlotCount_);
  return QRect(clampedIndex * slotWidth, 0, slotWidth, filmstrip_.height());
}

qint64 TimelineWidget::clampedPosition(qint64 positionMs) const
{
  return std::clamp<qint64>(positionMs, 0, durationMs_);
}

qint64 TimelineWidget::positionForX(int x) const
{
  if (durationMs_ <= 0) {
    return 0;
  }

  const QRectF groove = grooveRect();
  const double clampedX = std::clamp<double>(x, groove.left(), groove.right());
  const double ratio = (clampedX - groove.left()) / groove.width();
  return clampedPosition(static_cast<qint64>(ratio * static_cast<double>(durationMs_)));
}

int TimelineWidget::xForPosition(qint64 positionMs) const
{
  const QRectF groove = grooveRect();
  if (durationMs_ <= 0) {
    return static_cast<int>(groove.left());
  }

  const double ratio = static_cast<double>(clampedPosition(positionMs)) / static_cast<double>(durationMs_);
  return static_cast<int>(groove.left() + ratio * groove.width());
}

TimelineWidget::DragMode TimelineWidget::dragModeForPosition(const QPoint& position) const
{
  const QRectF filmstrip = filmstripRect();
  const QRectF groove = grooveRect();
  const QRectF interactiveZone = filmstrip.united(groove).adjusted(-12, -12, 12, 12);
  if (!interactiveZone.contains(position)) {
    return DragMode::Scrub;
  }

  const int samplingStartX = xForPosition(samplingStartMs_);
  const QRect sampleHotspot(
    samplingStartX - kSamplingMarkerHitRadiusPx,
    static_cast<int>(filmstrip.top()) - 16,
    kSamplingMarkerHitRadiusPx * 2,
    24);
  if (sampleHotspot.contains(position)) {
    return DragMode::SamplingStart;
  }

  const int startX = xForPosition(rangeStartMs_);
  if (std::abs(position.x() - startX) <= kHandleHitRadiusPx) {
    return DragMode::RangeStart;
  }

  const int endX = xForPosition(rangeEndMs_);
  if (std::abs(position.x() - endX) <= kHandleHitRadiusPx) {
    return DragMode::RangeEnd;
  }

  return DragMode::Scrub;
}

qint64 TimelineWidget::majorTickStepMs() const
{
  static constexpr std::array<qint64, 14> kCandidates = {
    40, 100, 200, 500, 1000, 2000, 5000, 10000, 15000, 30000, 60000, 120000, 300000, 600000
  };

  const QRectF groove = grooveRect();
  for (const qint64 candidate : kCandidates) {
    const double tickCount = static_cast<double>(durationMs_) / static_cast<double>(candidate);
    if (tickCount <= 0.0 || groove.width() / tickCount >= 72.0) {
      return candidate;
    }
  }

  return kCandidates.back();
}

QString TimelineWidget::formatTime(qint64 positionMs) const
{
  const qint64 clamped = std::max<qint64>(0, positionMs);
  const qint64 totalSeconds = clamped / 1000;
  const qint64 minutes = totalSeconds / 60;
  const qint64 seconds = totalSeconds % 60;
  const qint64 milliseconds = clamped % 1000;
  return QStringLiteral("%1:%2.%3")
    .arg(minutes)
    .arg(seconds, 2, 10, QLatin1Char('0'))
    .arg(milliseconds, 3, 10, QLatin1Char('0'));
}
