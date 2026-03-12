#pragma once

#include <optional>

#include <QDialog>
#include <QMainWindow>
#include <QRectF>

#include "ProjectInfo.hpp"
#include "RustBridge.hpp"

class QLabel;
class QPushButton;
class QDoubleSpinBox;
class QSpinBox;
class QTextEdit;
class QPixmap;

class CropOverlayWidget;
class FitPreviewWidget;
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
  void beginPreviewRender();
  void beginCropSelection();
  void applyPendingCrop();
  void clearCrop();
  void updateCropUi();
  void toggleDevOverlay();
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
  QLabel* cropStatusLabel_ = nullptr;
  QLabel* backendLabel_ = nullptr;
  QLabel* shellLabel_ = nullptr;
  QLabel* projectBridgeLabel_ = nullptr;
  TimelineWidget* timelineWidget_ = nullptr;
  CropOverlayWidget* cropOverlay_ = nullptr;
  FitPreviewWidget* sheetPreviewWidget_ = nullptr;
  QDoubleSpinBox* customJumpSecondsSpin_ = nullptr;
  QSpinBox* frameStepSpin_ = nullptr;
  QPushButton* selectCropButton_ = nullptr;
  QPushButton* applyCropButton_ = nullptr;
  QPushButton* clearCropButton_ = nullptr;
  QTextEdit* statusText_ = nullptr;
  QDialog* devOverlay_ = nullptr;
  ProjectInfo projectInfo_;
  QString sheetPreviewPath_;
  int loadRequestId_ = 0;
  int previewRequestId_ = 0;
  std::optional<QRectF> appliedCrop_;
};
