#pragma once

#include <QPixmap>
#include <QWidget>

/**
 * Letterboxed preview surface that fits an image inside the available bounds without letting the
 * image's native size distort the surrounding layout.
 */
class FitPreviewWidget final : public QWidget {
  Q_OBJECT

public:
  explicit FitPreviewWidget(QWidget* parent = nullptr);

  void setPreviewImage(const QPixmap& pixmap);
  void clearPreview(const QString& message);

protected:
  void paintEvent(QPaintEvent* event) override;

private:
  QPixmap previewPixmap_;
  QString placeholderText_;
};
