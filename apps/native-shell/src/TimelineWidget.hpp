#pragma once

#include <QElapsedTimer>
#include <QTimer>
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
  void setFramesPerSecond(double fps);

  [[nodiscard]] qint64 durationMs() const;
  [[nodiscard]] qint64 positionMs() const;

signals:
  void scrubPreviewRequested(qint64 positionMs);
  void scrubFinished(qint64 positionMs);

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;

private:
  void beginScrub(const QPoint& position);
  void endScrub(const QPoint& position);
  void updateScrub(const QPoint& position, bool forceDispatch);
  void dispatchPendingPreview();
  [[nodiscard]] QRectF grooveRect() const;
  [[nodiscard]] qint64 clampedPosition(qint64 positionMs) const;
  [[nodiscard]] qint64 positionForX(int x) const;
  [[nodiscard]] int xForPosition(qint64 positionMs) const;
  [[nodiscard]] qint64 majorTickStepMs() const;
  [[nodiscard]] QString formatTime(qint64 positionMs) const;

  QTimer previewTimer_;
  QElapsedTimer previewClock_;
  qint64 durationMs_ = 0;
  qint64 positionMs_ = 0;
  qint64 dragPositionMs_ = 0;
  qint64 pendingPreviewMs_ = -1;
  qint64 lastDispatchedPreviewMs_ = -1;
  double fps_ = 0.0;
  bool dragging_ = false;
  bool hovering_ = false;
  qint64 hoverPositionMs_ = 0;
};
