#include "InteractiveSheetWidget.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

#include <algorithm>

namespace {

QRectF fitRectWithAspect(const QRectF& bounds, double aspectRatio)
{
  const double safeAspect = std::max(0.05, aspectRatio);
  double width = bounds.width();
  double height = width / safeAspect;
  if (height > bounds.height()) {
    height = bounds.height();
    width = height * safeAspect;
  }

  return QRectF(
    bounds.center().x() - width / 2.0,
    bounds.center().y() - height / 2.0,
    width,
    height);
}

QSizeF renderCanvasSize(const SheetLayoutPreviewState& state)
{
  const double scale = std::max(0.25, state.exportScale);
  const double safeAspect = std::max(0.25, state.sourceAspectRatio);
  double cellWidth = std::max(96.0, std::round(240.0 * scale));
  double cellHeight = cellWidth / safeAspect;
  if (cellHeight > 240.0 * scale) {
    cellHeight = std::round(240.0 * scale);
    cellWidth = cellHeight * safeAspect;
  }

  const double gutter = std::round(state.gutterPx * scale);
  const double outer = std::round(state.outerMarginPx * scale);
  const double metadataHeight = state.showMetadataBar ? std::round(72.0 * scale) : 0.0;
  const double canvasWidth = outer * 2.0 + state.columns * cellWidth + std::max(0, state.columns - 1) * gutter;
  const double canvasHeight = outer * 2.0 + metadataHeight + state.rows * cellHeight + std::max(0, state.rows - 1) * gutter;
  return QSizeF(canvasWidth, canvasHeight);
}

} // namespace

InteractiveSheetWidget::InteractiveSheetWidget(QWidget* parent)
  : QWidget(parent)
{
  setMinimumHeight(320);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setMouseTracking(true);
  placeholderText_ = "Load a video to render the interactive sheet preview.";
}

void InteractiveSheetWidget::setPreviewImage(const QPixmap& pixmap)
{
  previewPixmap_ = pixmap;
  placeholderText_.clear();
  update();
}

void InteractiveSheetWidget::clearPreview(const QString& message)
{
  previewPixmap_ = QPixmap();
  placeholderText_ = message;
  hoveredTileId_.clear();
  update();
}

void InteractiveSheetWidget::setLayoutState(const SheetLayoutPreviewState& state)
{
  layoutState_ = state;
  update();
}

void InteractiveSheetWidget::setSelectedTileId(const QString& tileId)
{
  if (selectedTileId_ == tileId) {
    return;
  }

  selectedTileId_ = tileId;
  update();
}

QString InteractiveSheetWidget::selectedTileId() const
{
  return selectedTileId_;
}

void InteractiveSheetWidget::paintEvent(QPaintEvent* event)
{
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const QRect frameRect = rect().adjusted(1, 1, -1, -1);
  painter.setPen(QColor("#273647"));
  painter.setBrush(QColor("#0d1319"));
  painter.drawRoundedRect(frameRect, 10, 10);

  if (previewPixmap_.isNull()) {
    painter.setPen(QColor("#aab8c4"));
    painter.drawText(frameRect.adjusted(20, 20, -20, -20), Qt::AlignCenter | Qt::TextWordWrap, placeholderText_);
    return;
  }

  const QRect drawRect = imageDrawRect();
  painter.drawPixmap(drawRect, previewPixmap_);

  const QHash<QString, QRectF> rects = tileRects();
  for (auto it = rects.constBegin(); it != rects.constEnd(); ++it) {
    const bool isSelected = it.key() == selectedTileId_;
    const bool isHovered = it.key() == hoveredTileId_;
    QColor outline = QColor("#87b8e4");
    if (isSelected) {
      outline = QColor("#f0d46b");
    } else if (isHovered) {
      outline = QColor("#7ae0a1");
    }

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(outline, isSelected ? 2.0 : 1.5));
    painter.drawRoundedRect(it.value(), 8, 8);
  }
}

void InteractiveSheetWidget::mousePressEvent(QMouseEvent* event)
{
  const QHash<QString, QRectF> rects = tileRects();
  for (auto it = rects.constBegin(); it != rects.constEnd(); ++it) {
    if (!it.value().contains(event->position())) {
      continue;
    }

    selectedTileId_ = it.key();
    update();
    emit tileSelected(it.key());
    event->accept();
    return;
  }

  QWidget::mousePressEvent(event);
}

void InteractiveSheetWidget::mouseMoveEvent(QMouseEvent* event)
{
  QString nextHovered;
  const QHash<QString, QRectF> rects = tileRects();
  for (auto it = rects.constBegin(); it != rects.constEnd(); ++it) {
    if (it.value().contains(event->position())) {
      nextHovered = it.key();
      break;
    }
  }

  if (hoveredTileId_ != nextHovered) {
    hoveredTileId_ = nextHovered;
    update();
  }

  QWidget::mouseMoveEvent(event);
}

void InteractiveSheetWidget::leaveEvent(QEvent* event)
{
  hoveredTileId_.clear();
  update();
  QWidget::leaveEvent(event);
}

QRect InteractiveSheetWidget::imageDrawRect() const
{
  const QRect contentRect = rect().adjusted(16, 16, -16, -16);
  if (previewPixmap_.isNull()) {
    return contentRect;
  }

  const QPixmap scaled = previewPixmap_.scaled(
    contentRect.size(),
    Qt::KeepAspectRatio,
    Qt::SmoothTransformation);
  return QRect(
    contentRect.x() + (contentRect.width() - scaled.width()) / 2,
    contentRect.y() + (contentRect.height() - scaled.height()) / 2,
    scaled.width(),
    scaled.height());
}

QHash<QString, QRectF> InteractiveSheetWidget::tileRects() const
{
  QHash<QString, QRectF> rects;
  if (previewPixmap_.isNull() || layoutState_.tiles.isEmpty()) {
    return rects;
  }

  const QSizeF canvas = renderCanvasSize(layoutState_);
  if (canvas.width() <= 0.0 || canvas.height() <= 0.0) {
    return rects;
  }

  const QRect drawRect = imageDrawRect();
  const double imageScaleX = drawRect.width() / canvas.width();
  const double imageScaleY = drawRect.height() / canvas.height();
  const double scale = std::max(0.25, layoutState_.exportScale);
  const double safeAspect = std::max(0.25, layoutState_.sourceAspectRatio);
  double cellWidth = std::max(96.0, std::round(240.0 * scale));
  double cellHeight = cellWidth / safeAspect;
  if (cellHeight > 240.0 * scale) {
    cellHeight = std::round(240.0 * scale);
    cellWidth = cellHeight * safeAspect;
  }

  const double gutter = std::round(layoutState_.gutterPx * scale);
  const double outer = std::round(layoutState_.outerMarginPx * scale);
  const double metadataHeight = layoutState_.showMetadataBar ? std::round(72.0 * scale) : 0.0;

  for (const SheetTileLayoutState& tile : layoutState_.tiles) {
    const double x = outer + tile.column * (cellWidth + gutter);
    const double y = outer + metadataHeight + tile.row * (cellHeight + gutter);
    const double width = tile.columnSpan * cellWidth + std::max(0, tile.columnSpan - 1) * gutter;
    const double height = tile.rowSpan * cellHeight + std::max(0, tile.rowSpan - 1) * gutter;

    rects.insert(
      tile.id,
      QRectF(
        drawRect.left() + x * imageScaleX,
        drawRect.top() + y * imageScaleY,
        width * imageScaleX,
        height * imageScaleY));
  }

  return rects;
}
