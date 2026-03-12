#include "FitPreviewWidget.hpp"

#include <QPainter>
#include <QPaintEvent>

FitPreviewWidget::FitPreviewWidget(QWidget* parent)
  : QWidget(parent)
{
  setMinimumHeight(320);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  placeholderText_ = "Load a video to render the sheet preview.";
}

void FitPreviewWidget::setPreviewImage(const QPixmap& pixmap)
{
  previewPixmap_ = pixmap;
  placeholderText_.clear();
  update();
}

void FitPreviewWidget::clearPreview(const QString& message)
{
  previewPixmap_ = QPixmap();
  placeholderText_ = message;
  update();
}

void FitPreviewWidget::paintEvent(QPaintEvent* event)
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

  const QRect contentRect = frameRect.adjusted(16, 16, -16, -16);
  const QPixmap scaled = previewPixmap_.scaled(
    contentRect.size(),
    Qt::KeepAspectRatio,
    Qt::SmoothTransformation);
  const QPoint topLeft(
    contentRect.x() + (contentRect.width() - scaled.width()) / 2,
    contentRect.y() + (contentRect.height() - scaled.height()) / 2);
  painter.drawPixmap(topLeft, scaled);
}
