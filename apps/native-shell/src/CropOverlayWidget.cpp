#include "CropOverlayWidget.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QApplication>
#include <QEvent>

#include <algorithm>

namespace {

constexpr qreal kMinimumSelectionSizePx = 6.0;

} // namespace

CropOverlayWidget::CropOverlayWidget(QWidget* parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_TransparentForMouseEvents, true);
  setAttribute(Qt::WA_NoSystemBackground, true);
  setMouseTracking(true);
}

void CropOverlayWidget::setSourceVideoSize(const QSize& sourceVideoSize)
{
  sourceVideoSize_ = sourceVideoSize;
  update();
}

void CropOverlayWidget::beginSelection()
{
  selectionMode_ = true;
  selecting_ = false;
  pendingCrop_.reset();
  raise();
  show();
  setCursor(Qt::CrossCursor);
  qApp->installEventFilter(this);
  updateMouseTransparency();
  emit pendingCropChanged(false);
  emit selectionModeChanged(true);
  update();
}

void CropOverlayWidget::cancelSelection()
{
  selectionMode_ = false;
  selecting_ = false;
  pendingCrop_.reset();
  qApp->removeEventFilter(this);
  unsetCursor();
  updateMouseTransparency();
  emit pendingCropChanged(false);
  emit selectionModeChanged(false);
  update();
}

void CropOverlayWidget::clearPendingCrop()
{
  pendingCrop_.reset();
  emit pendingCropChanged(false);
  update();
}

void CropOverlayWidget::setAppliedCrop(const std::optional<QRectF>& crop)
{
  appliedCrop_ = crop;
  if (!selectionMode_) {
    pendingCrop_.reset();
    emit pendingCropChanged(false);
  }
  update();
}

std::optional<QRectF> CropOverlayWidget::pendingCrop() const
{
  return pendingCrop_;
}

std::optional<QRectF> CropOverlayWidget::appliedCrop() const
{
  return appliedCrop_;
}

bool CropOverlayWidget::eventFilter(QObject* watched, QEvent* event)
{
  Q_UNUSED(watched);
  if (!selectionMode_) {
    return QWidget::eventFilter(watched, event);
  }

  switch (event->type()) {
  case QEvent::MouseButtonPress: {
    auto* mouseEvent = static_cast<QMouseEvent*>(event);
    if (mouseEvent->button() == Qt::LeftButton) {
      handlePress(mapFromGlobal(mouseEvent->globalPosition().toPoint()));
      return selecting_;
    }
    return false;
  }
  case QEvent::MouseMove: {
    auto* mouseEvent = static_cast<QMouseEvent*>(event);
    if (selecting_) {
      handleMove(mapFromGlobal(mouseEvent->globalPosition().toPoint()));
      return true;
    }
    return false;
  }
  case QEvent::MouseButtonRelease: {
    auto* mouseEvent = static_cast<QMouseEvent*>(event);
    if (selecting_ && mouseEvent->button() == Qt::LeftButton) {
      handleRelease(mapFromGlobal(mouseEvent->globalPosition().toPoint()));
      return true;
    }
    return false;
  }
  default:
    return QWidget::eventFilter(watched, event);
  }
}

void CropOverlayWidget::paintEvent(QPaintEvent* event)
{
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const QRectF videoRect = fittedVideoRect();
  if (videoRect.isEmpty()) {
    return;
  }

  painter.setPen(QPen(QColor("#273647"), 1));
  painter.setBrush(Qt::NoBrush);
  painter.drawRoundedRect(videoRect, 8, 8);

  const std::optional<QRectF> activeCrop = pendingCrop_.has_value() ? pendingCrop_ : appliedCrop_;
  if (activeCrop.has_value()) {
    const QRectF cropRect = displayRectForNormalizedCrop(*activeCrop);
    const QColor dimColor = pendingCrop_.has_value()
      ? QColor(0, 0, 0, 110)
      : QColor(0, 0, 0, 140);

    painter.fillRect(QRectF(videoRect.left(), videoRect.top(), videoRect.width(), cropRect.top() - videoRect.top()), dimColor);
    painter.fillRect(QRectF(videoRect.left(), cropRect.bottom(), videoRect.width(), videoRect.bottom() - cropRect.bottom()), dimColor);
    painter.fillRect(QRectF(videoRect.left(), cropRect.top(), cropRect.left() - videoRect.left(), cropRect.height()), dimColor);
    painter.fillRect(QRectF(cropRect.right(), cropRect.top(), videoRect.right() - cropRect.right(), cropRect.height()), dimColor);

    painter.setPen(QPen(pendingCrop_.has_value() ? QColor("#f0d46b") : QColor("#7dd3fc"), 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(cropRect, 6, 6);
  }

  if (selectionMode_) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(14, 20, 28, 180));
    const QRectF helperRect(videoRect.left() + 12, videoRect.top() + 12, 240, 30);
    painter.drawRoundedRect(helperRect, 8, 8);
    painter.setPen(QColor("#f4f7fb"));
    painter.drawText(helperRect.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft, "Drag to select crop area");
  }
}

void CropOverlayWidget::mousePressEvent(QMouseEvent* event)
{
  QWidget::mousePressEvent(event);
}

void CropOverlayWidget::mouseMoveEvent(QMouseEvent* event)
{
  QWidget::mouseMoveEvent(event);
}

void CropOverlayWidget::mouseReleaseEvent(QMouseEvent* event)
{
  QWidget::mouseReleaseEvent(event);
}

void CropOverlayWidget::handlePress(const QPointF& position)
{
  if (fittedVideoRect().isEmpty()) {
    return;
  }

  selecting_ = true;
  dragStart_ = clampToVideoRect(position);
  pendingCrop_.reset();
  emit pendingCropChanged(false);
  update();
}

void CropOverlayWidget::handleMove(const QPointF& position)
{
  if (!selectionMode_ || !selecting_) {
    return;
  }

  const QRectF crop = normalizedRectFromPoints(dragStart_, clampToVideoRect(position));
  if (crop.width() > 0.0 && crop.height() > 0.0) {
    pendingCrop_ = crop;
    emit pendingCropChanged(true);
  }
  update();
}

void CropOverlayWidget::handleRelease(const QPointF& position)
{
  if (!selectionMode_ || !selecting_) {
    return;
  }

  selecting_ = false;
  const QRectF displayRect = QRectF(dragStart_, clampToVideoRect(position)).normalized();
  if (displayRect.width() < kMinimumSelectionSizePx || displayRect.height() < kMinimumSelectionSizePx) {
    pendingCrop_.reset();
    emit pendingCropChanged(false);
  } else {
    pendingCrop_ = normalizedRectFromPoints(dragStart_, clampToVideoRect(position));
    emit pendingCropChanged(true);
  }

  selectionMode_ = false;
  qApp->removeEventFilter(this);
  unsetCursor();
  updateMouseTransparency();
  emit selectionModeChanged(false);
  update();
}

void CropOverlayWidget::updateMouseTransparency()
{
  setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

QRectF CropOverlayWidget::fittedVideoRect() const
{
  if (sourceVideoSize_.isEmpty() || width() <= 0 || height() <= 0) {
    return {};
  }

  const qreal sourceAspect = static_cast<qreal>(sourceVideoSize_.width()) / static_cast<qreal>(sourceVideoSize_.height());
  const qreal widgetAspect = static_cast<qreal>(width()) / static_cast<qreal>(height());

  qreal renderWidth = width();
  qreal renderHeight = height();
  if (widgetAspect > sourceAspect) {
    renderWidth = renderHeight * sourceAspect;
  } else {
    renderHeight = renderWidth / sourceAspect;
  }

  return QRectF(
    (width() - renderWidth) / 2.0,
    (height() - renderHeight) / 2.0,
    renderWidth,
    renderHeight
  );
}

QPointF CropOverlayWidget::clampToVideoRect(const QPointF& point) const
{
  const QRectF videoRect = fittedVideoRect();
  return QPointF(
    std::clamp(point.x(), videoRect.left(), videoRect.right()),
    std::clamp(point.y(), videoRect.top(), videoRect.bottom())
  );
}

QRectF CropOverlayWidget::normalizedRectFromPoints(const QPointF& start, const QPointF& end) const
{
  const QRectF videoRect = fittedVideoRect();
  const QRectF selectionRect(start, end);
  const QRectF normalized = selectionRect.normalized();

  return QRectF(
    (normalized.left() - videoRect.left()) / videoRect.width(),
    (normalized.top() - videoRect.top()) / videoRect.height(),
    normalized.width() / videoRect.width(),
    normalized.height() / videoRect.height()
  );
}

QRectF CropOverlayWidget::displayRectForNormalizedCrop(const QRectF& normalizedCrop) const
{
  const QRectF videoRect = fittedVideoRect();
  return QRectF(
    videoRect.left() + normalizedCrop.x() * videoRect.width(),
    videoRect.top() + normalizedCrop.y() * videoRect.height(),
    normalizedCrop.width() * videoRect.width(),
    normalizedCrop.height() * videoRect.height()
  );
}
