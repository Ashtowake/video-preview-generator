#pragma once

#include <QWidget>

/**
 * Scrubbable transport timeline with local drag feedback and throttled preview seeks.
 *
 * The widget keeps drag interaction separate from the player's authoritative playback state so
 * the UI can stay responsive while aggressive scrubbing is coalesced into preview seeks.
 */
class TimelineWidget final : public QWidget {
  Q_OBJECT

public:
  explicit TimelineWidget(QWidget* parent = nullptr);

  void setDurationMs(qint64 durationMs);
  void setPositionMs(qint64 positionMs);
  void setSelectionRangeMs(qint64 startMs, qint64 endMs);
  void setFramesPerSecond(double fps);

  [[nodiscard]] qint64 durationMs() const;
  [[nodiscard]] qint64 positionMs() const;
  [[nodiscard]] qint64 selectionRangeStartMs() const;
  [[nodiscard]] qint64 selectionRangeEndMs() const;
  [[nodiscard]] bool isScrubbing() const;

signals:
  void scrubPreviewRequested(qint64 positionMs);
  void scrubFinished(qint64 positionMs);
  void rangePreviewChanged(qint64 startMs, qint64 endMs);
  void rangeChangeFinished(qint64 startMs, qint64 endMs);

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;

private:
  enum class DragMode {
    None,
    Scrub,
    RangeStart,
    RangeEnd,
  };

  void beginDrag(const QPoint& position, DragMode mode);
  void endDrag(const QPoint& position);
  void updateDrag(const QPoint& position, bool forceDispatch);
  void dispatchPendingPreview();
  [[nodiscard]] QRectF grooveRect() const;
  [[nodiscard]] qint64 clampedPosition(qint64 positionMs) const;
  [[nodiscard]] qint64 positionForX(int x) const;
  [[nodiscard]] int xForPosition(qint64 positionMs) const;
  [[nodiscard]] DragMode dragModeForPosition(const QPoint& position) const;
  [[nodiscard]] qint64 majorTickStepMs() const;
  [[nodiscard]] QString formatTime(qint64 positionMs) const;

  qint64 durationMs_ = 0;
  qint64 positionMs_ = 0;
  qint64 dragPositionMs_ = 0;
  qint64 rangeStartMs_ = 0;
  qint64 rangeEndMs_ = 0;
  qint64 pendingPreviewMs_ = -1;
  qint64 lastDispatchedPreviewMs_ = -1;
  double fps_ = 0.0;
  bool dragging_ = false;
  bool hovering_ = false;
  qint64 hoverPositionMs_ = 0;
  DragMode dragMode_ = DragMode::None;
};
