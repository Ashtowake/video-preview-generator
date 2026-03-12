#pragma once

#include <optional>

#include <QRectF>
#include <QSize>
#include <QWidget>

/**
 * Interactive crop overlay drawn above the mpv transport surface.
 *
 * The overlay stays visually aligned with the fitted video rectangle inside the player and emits
 * normalized crop rectangles so the native shell and Rust renderer share the same crop model.
 */
class CropOverlayWidget final : public QWidget {
  Q_OBJECT

public:
  explicit CropOverlayWidget(QWidget* parent = nullptr);

  void setSourceVideoSize(const QSize& sourceVideoSize);
  void beginSelection();
  void cancelSelection();
  void clearPendingCrop();
  void setAppliedCrop(const std::optional<QRectF>& crop);

  [[nodiscard]] std::optional<QRectF> pendingCrop() const;
  [[nodiscard]] std::optional<QRectF> appliedCrop() const;

signals:
  void pendingCropChanged(bool hasPendingCrop);
  void selectionModeChanged(bool active);

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

private:
  void handlePress(const QPointF& position);
  void handleMove(const QPointF& position);
  void handleRelease(const QPointF& position);
  void updateMouseTransparency();
  [[nodiscard]] QRectF fittedVideoRect() const;
  [[nodiscard]] QPointF clampToVideoRect(const QPointF& point) const;
  [[nodiscard]] QRectF normalizedRectFromPoints(const QPointF& start, const QPointF& end) const;
  [[nodiscard]] QRectF displayRectForNormalizedCrop(const QRectF& normalizedCrop) const;

  QSize sourceVideoSize_;
  QPointF dragStart_;
  std::optional<QRectF> pendingCrop_;
  std::optional<QRectF> appliedCrop_;
  bool selectionMode_ = false;
  bool selecting_ = false;
};
