#pragma once

#include <QHash>
#include <QPixmap>
#include <QWidget>

#include "SheetLayoutState.hpp"

/**
 * Letterboxed sheet preview with clickable tile overlays.
 *
 * The widget uses the same grid geometry as the Rust renderer so tile selection lines up with the
 * rendered preview image even when the preview is downscaled for interactive use.
 */
class InteractiveSheetWidget final : public QWidget {
  Q_OBJECT

public:
  explicit InteractiveSheetWidget(QWidget* parent = nullptr);

  void setPreviewImage(const QPixmap& pixmap);
  void clearPreview(const QString& message);
  void setLayoutState(const SheetLayoutPreviewState& state);
  void setSelectedTileId(const QString& tileId);
  [[nodiscard]] QString selectedTileId() const;

signals:
  void tileSelected(const QString& tileId);

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;

private:
  [[nodiscard]] QRect imageDrawRect() const;
  [[nodiscard]] QHash<QString, QRectF> tileRects() const;

  QPixmap previewPixmap_;
  QString placeholderText_;
  SheetLayoutPreviewState layoutState_;
  QString selectedTileId_;
  QString hoveredTileId_;
};
