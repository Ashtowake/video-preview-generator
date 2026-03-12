#include "TimelineWidget.hpp"

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>

#include <algorithm>
#include <array>

namespace {

constexpr int kPreviewSeekIntervalMs = 6;

} // namespace

TimelineWidget::TimelineWidget(QWidget* parent)
  : QWidget(parent)
{
  setMinimumHeight(94);
  setMouseTracking(true);

  previewTimer_.setInterval(kPreviewSeekIntervalMs);
  previewTimer_.setSingleShot(true);
  connect(&previewTimer_, &QTimer::timeout, this, &TimelineWidget::dispatchPendingPreview);
}

void TimelineWidget::setDurationMs(qint64 durationMs)
{
  durationMs_ = std::max<qint64>(0, durationMs);
  positionMs_ = clampedPosition(positionMs_);
  dragPositionMs_ = clampedPosition(dragPositionMs_);
  hoverPositionMs_ = clampedPosition(hoverPositionMs_);
  update();
}

void TimelineWidget::setPositionMs(qint64 positionMs)
{
  positionMs_ = clampedPosition(positionMs);
  if (!dragging_) {
    dragPositionMs_ = positionMs_;
  }
  update();
}

void TimelineWidget::setFramesPerSecond(double fps)
{
  fps_ = fps;
  update();
}

qint64 TimelineWidget::durationMs() const
{
  return durationMs_;
}

qint64 TimelineWidget::positionMs() const
{
  return dragging_ ? dragPositionMs_ : positionMs_;
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

  const QRectF groove = grooveRect();
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor("#0b1016"));
  painter.drawRoundedRect(groove, 7, 7);

  if (durationMs_ <= 0) {
    painter.setPen(QColor("#8ea1b3"));
    painter.drawText(frame.adjusted(16, 16, -16, -16), Qt::AlignCenter, "Timeline becomes active after loading a video.");
    return;
  }

  painter.setClipRect(groove);
  const qint64 currentPositionMs = positionMs();
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
  painter.drawLine(QPointF(playheadX, groove.top() - 10), QPointF(playheadX, groove.bottom() + 10));
  painter.setBrush(QColor("#f0d46b"));
  painter.setPen(Qt::NoPen);
  painter.drawEllipse(QPointF(playheadX, groove.center().y()), 6, 6);

  painter.setPen(QColor("#e8edf3"));
  painter.drawText(QRectF(16, 10, width() - 32, 16), Qt::AlignLeft | Qt::AlignVCenter, formatTime(currentPositionMs));

  QString timelineHint = QStringLiteral("Duration %1").arg(formatTime(durationMs_));
  if (fps_ > 0.0) {
    timelineHint.append(QStringLiteral("  |  %1 fps").arg(fps_, 0, 'f', 2));
  }
  painter.drawText(QRectF(16, 10, width() - 32, 16), Qt::AlignRight | Qt::AlignVCenter, timelineHint);

  if (hovering_ && !dragging_) {
    const int hoverX = xForPosition(hoverPositionMs_);
    painter.setPen(QPen(QColor("#8bbdf0"), 1));
    painter.drawLine(QPointF(hoverX, groove.top() - 4), QPointF(hoverX, groove.bottom() + 4));
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
    updateScrub(mapFromGlobal(mouseEvent->globalPosition().toPoint()), false);
    return false;
  }
  case QEvent::MouseButtonRelease: {
    auto* mouseEvent = static_cast<QMouseEvent*>(event);
    if (mouseEvent->button() == Qt::LeftButton) {
      endScrub(mapFromGlobal(mouseEvent->globalPosition().toPoint()));
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

  beginScrub(event->position().toPoint());
  event->accept();
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* event)
{
  hoverPositionMs_ = positionForX(event->position().toPoint().x());
  hovering_ = rect().contains(event->position().toPoint());

  if (dragging_) {
    updateScrub(event->position().toPoint(), false);
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

  endScrub(event->position().toPoint());
  event->accept();
}

void TimelineWidget::leaveEvent(QEvent* event)
{
  hovering_ = false;
  update();
  QWidget::leaveEvent(event);
}

void TimelineWidget::beginScrub(const QPoint& position)
{
  dragging_ = true;
  qApp->installEventFilter(this);
  previewClock_.invalidate();
  pendingPreviewMs_ = -1;
  lastDispatchedPreviewMs_ = -1;
  updateScrub(position, true);
}

void TimelineWidget::endScrub(const QPoint& position)
{
  updateScrub(position, true);
  dragging_ = false;
  qApp->removeEventFilter(this);
  previewTimer_.stop();
  pendingPreviewMs_ = -1;
  lastDispatchedPreviewMs_ = -1;
  emit scrubFinished(dragPositionMs_);
  update();
}

void TimelineWidget::updateScrub(const QPoint& position, bool forceDispatch)
{
  dragPositionMs_ = positionForX(position.x());
  update();

  if (forceDispatch) {
    pendingPreviewMs_ = dragPositionMs_;
    dispatchPendingPreview();
    return;
  }

  pendingPreviewMs_ = dragPositionMs_;
  if (!previewClock_.isValid() || previewClock_.elapsed() >= kPreviewSeekIntervalMs) {
    dispatchPendingPreview();
    return;
  }

  if (!previewTimer_.isActive()) {
    previewTimer_.start(kPreviewSeekIntervalMs - static_cast<int>(previewClock_.elapsed()));
  }
}

void TimelineWidget::dispatchPendingPreview()
{
  if (pendingPreviewMs_ < 0 || pendingPreviewMs_ == lastDispatchedPreviewMs_) {
    return;
  }

  lastDispatchedPreviewMs_ = pendingPreviewMs_;
  previewClock_.restart();
  emit scrubPreviewRequested(pendingPreviewMs_);
}

QRectF TimelineWidget::grooveRect() const
{
  return QRectF(16.0, 30.0, std::max(32, width() - 32), 18.0);
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
