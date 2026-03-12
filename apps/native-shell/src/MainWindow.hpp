#pragma once

#include <QMainWindow>

#include "ProjectInfo.hpp"
#include "RustBridge.hpp"

class QLabel;
class QDoubleSpinBox;
class QSpinBox;
class QTextEdit;
class QPixmap;

class MpvWidget;
class TimelineWidget;

/**
 * Native editor shell focused on the workflow-critical transport path first.
 *
 * The layout mirrors an editor-style split: transport on the left, sheet/inspector workspace on
 * the right. Sheet composition and project editing migrate in after the playback foundation.
 */
class MainWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget* parent = nullptr);

private:
  void beginBackgroundLoad(const QString& path);
  void openVideo();
  void updateMetadata(const ProjectInfo& info);
  void updateTransport(qint64 positionMs, qint64 durationMs);
  void refreshSheetPreview();
  void showSheetPreview(const QString& imagePath);
  void applyDarkPalette();
  void createUi();
  void createMenuBar();
  void appendStatusMessage(const QString& message);
  [[nodiscard]] QString formatTime(qint64 timeMs) const;
  [[nodiscard]] qint64 customJumpMs() const;
  [[nodiscard]] int displayFrameNumber(qint64 positionMs) const;

  RustBridge rustBridge_;
  MpvWidget* mpvWidget_ = nullptr;
  QLabel* titleLabel_ = nullptr;
  QLabel* infoLabel_ = nullptr;
  QLabel* timeLabel_ = nullptr;
  QLabel* frameLabel_ = nullptr;
  QLabel* backendLabel_ = nullptr;
  QLabel* sheetPreviewLabel_ = nullptr;
  TimelineWidget* timelineWidget_ = nullptr;
  QDoubleSpinBox* customJumpSecondsSpin_ = nullptr;
  QSpinBox* frameStepSpin_ = nullptr;
  QTextEdit* statusText_ = nullptr;
  ProjectInfo projectInfo_;
  QString sheetPreviewPath_;
  int loadRequestId_ = 0;
};
