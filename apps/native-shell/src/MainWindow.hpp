#pragma once

#include <optional>

#include <QCloseEvent>
#include <QDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QJsonObject>
#include <QMainWindow>
#include <QRectF>
#include <QString>
#include <QVector>

#include "ProjectInfo.hpp"
#include "RustBridge.hpp"
#include "SheetLayoutState.hpp"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
class QSplitter;
class QStatusBar;
class QTabWidget;
class QTableWidget;
class QTextEdit;
class QTreeWidget;
class QTreeWidgetItem;

class CropOverlayWidget;
class InteractiveSheetWidget;
class MpvWidget;
class TimelineWidget;

struct ReviewTileState {
  SheetTileLayoutState layout;
  bool pinned = false;
  qint64 frameIndex = 0;
  qint64 timeMs = 0;
  qint64 fineTuneOffsetMs = 0;
};

struct LayoutPresetState {
  QString id;
  QString name;
  QString filePath;
  int rows = 4;
  int columns = 5;
  int gutterPx = 12;
  int outerMarginPx = 24;
  int sharpnessWindow = 12;
  int frameRoundingPx = 20;
  int frameShadowPx = 18;
  int frameBorderPx = 2;
  bool showMetadataBar = true;
  bool showTimestamps = true;
  bool darkMode = true;
  QString watermarkText = QStringLiteral("Preview");
  double watermarkTextOpacity = 0.55;
  QString watermarkImagePath;
  double watermarkImageOpacity = 0.75;
  QString exportFormat = QStringLiteral("png");
  double exportScale = 1.0;
  QVector<SheetTileLayoutState> tiles;
};

struct ReviewSourceState {
  QString path;
  QString displayName;
  ProjectInfo info;
  bool loading = false;
  bool exported = false;
  QString lastError;
  QString appliedLayoutPresetId;
  QString selectedTileId;
  QString lastExportPath;
  qint64 rangeStartMs = 0;
  qint64 rangeEndMs = 0;
  qint64 samplingStartMs = 0;
  std::optional<QRectF> crop;
  QVector<ReviewTileState> tiles;
};

/**
 * Native editor shell organized around Review and Layout workspaces.
 *
 * Review keeps sources, transport, and per-video frame assignment in one place. Layouts owns
 * reusable sheet geometry and style presets that can be applied to one or many sources.
 */
class MainWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget* parent = nullptr);

protected:
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;
  void closeEvent(QCloseEvent* event) override;

private:
  void addVideos();
  void addSources(const QStringList& paths, bool activateLast);
  void loadVideo(const QString& path);
  void activateSourceByIndex(int index);
  void activateCurrentSourceFromSelection();
  void beginBackgroundLoad(int sourceIndex);
  void beginPreviewRender();
  void beginLayoutPreviewRender();
  void beginTimelineStripRender();
  void beginCropSelection();
  void applyPendingCrop();
  void clearCrop();
  void updateCropUi();
  void toggleDevOverlay();
  void updateMetadata(const ProjectInfo& info);
  void updateTransport(qint64 positionMs, qint64 durationMs);
  void updateSelectedRange(qint64 startMs, qint64 endMs, bool refreshPreview);
  void updateSamplingStart(qint64 samplingStartMs, bool refreshPreview);
  void refreshSheetPreview();
  void refreshLayoutPreview();
  void showSheetPreview(const QString& imagePath);
  void showLayoutPreview(const QString& imagePath);
  void playSelectedRange();
  void togglePlayback();
  void stopSelectedRange();
  void assignCurrentFrameToSelectedTile();
  void fineTuneSelectedTileFrames(int direction);
  void toggleSelectedTilePinned();
  void findSharpestForSelectedTile();
  void applyLayoutToCurrentSource();
  void applyLayoutToSelectedSources();
  void exportBatchQueue();
  void chooseBatchExportDirectory();
  void chooseWatermarkImage();
  void createNewLayoutPreset();
  void duplicateCurrentLayoutPreset();
  void renameCurrentLayoutPreset();
  void deleteCurrentLayoutPreset();
  void loadLayoutPresetFromFile();
  void saveCurrentLayoutPreset();
  void saveCurrentLayoutPresetAs();
  void selectLayoutPreset(int index);
  void syncLayoutEditorsFromCurrentPreset();
  void syncCurrentPresetFromEditors();
  void refreshLayoutPresetList();
  void refreshLayoutTilesTable();
  void updateLayoutControlsEnabled();
  void refreshSourceBin();
  void refreshSourceBinFilter();
  void updateSourceTreeItem(int index);
  void refreshQueuePanel();
  void updateQuickSheetControls();
  void updateWorkspaceLabels();
  void applyDarkPalette();
  void createUi();
  void createMenuBar();
  void appendStatusMessage(const QString& message);
  void saveUiState() const;
  void restoreUiState();
  [[nodiscard]] QString formatTime(qint64 timeMs) const;
  [[nodiscard]] qint64 customJumpMs() const;
  [[nodiscard]] int displayFrameNumber(qint64 positionMs) const;
  [[nodiscard]] int findSourceIndex(const QString& path) const;
  [[nodiscard]] int activeSourceIndex() const;
  [[nodiscard]] ReviewSourceState* activeSource();
  [[nodiscard]] const ReviewSourceState* activeSource() const;
  [[nodiscard]] LayoutPresetState* currentLayoutPreset();
  [[nodiscard]] const LayoutPresetState* currentLayoutPreset() const;
  [[nodiscard]] LayoutPresetState* layoutPresetById(const QString& id);
  [[nodiscard]] const LayoutPresetState* layoutPresetById(const QString& id) const;
  [[nodiscard]] const LayoutPresetState* appliedLayoutForSource(const ReviewSourceState& source) const;
  [[nodiscard]] LayoutPresetState defaultLayoutPreset() const;
  [[nodiscard]] QVector<SheetTileLayoutState> defaultTilesForGrid(int rows, int columns) const;
  void applyLayoutToSourceState(ReviewSourceState& source, const LayoutPresetState& preset, bool preservePinned);
  void reassignAutoTiles(ReviewSourceState& source);
  [[nodiscard]] qint64 seekTimeForFrameIndex(qint64 frameIndex, qint64 durationMs, double fps) const;
  [[nodiscard]] qint64 frameIndexAtTime(qint64 timeMs, double fps, qint64 frameCount) const;
  [[nodiscard]] SheetLayoutPreviewState sheetLayoutPreviewState(
    const LayoutPresetState& preset,
    const ReviewSourceState* source) const;
  [[nodiscard]] QJsonObject buildProjectJson(
    const ReviewSourceState& source,
    const LayoutPresetState& preset) const;
  void updateSourceFromProjectJson(ReviewSourceState& source, const QJsonObject& project);
  [[nodiscard]] QJsonObject buildLayoutPresetJson(const LayoutPresetState& preset) const;
  [[nodiscard]] LayoutPresetState parseLayoutPresetJson(const QJsonObject& object) const;
  void updateRecentVideos(const QString& path);
  void updateRecentLayouts(const QString& path);
  void rebuildRecentMenus();

  RustBridge rustBridge_;
  QTabWidget* workspaceTabs_ = nullptr;
  QSplitter* reviewRootSplitter_ = nullptr;
  QSplitter* reviewContentSplitter_ = nullptr;
  QWidget* queuePanel_ = nullptr;
  QWidget* sourceBinPanel_ = nullptr;
  MpvWidget* mpvWidget_ = nullptr;
  QLabel* titleLabel_ = nullptr;
  QLabel* infoLabel_ = nullptr;
  QLabel* timeLabel_ = nullptr;
  QLabel* frameLabel_ = nullptr;
  QLabel* rangeLabel_ = nullptr;
  QLabel* samplingStartLabel_ = nullptr;
  QLabel* cropStatusLabel_ = nullptr;
  QLabel* backendLabel_ = nullptr;
  QLabel* shellLabel_ = nullptr;
  QLabel* projectBridgeLabel_ = nullptr;
  QLabel* sourceBinStatusLabel_ = nullptr;
  QLabel* queueStatusLabel_ = nullptr;
  QLabel* selectedTileLabel_ = nullptr;
  QLabel* selectedTileDetailsLabel_ = nullptr;
  TimelineWidget* timelineWidget_ = nullptr;
  CropOverlayWidget* cropOverlay_ = nullptr;
  InteractiveSheetWidget* reviewSheetWidget_ = nullptr;
  InteractiveSheetWidget* layoutPreviewWidget_ = nullptr;
  QDoubleSpinBox* customJumpSecondsSpin_ = nullptr;
  QSpinBox* frameStepSpin_ = nullptr;
  QPushButton* selectCropButton_ = nullptr;
  QPushButton* applyCropButton_ = nullptr;
  QPushButton* clearCropButton_ = nullptr;
  QPushButton* assignFrameButton_ = nullptr;
  QPushButton* pinTileButton_ = nullptr;
  QPushButton* sharpestButton_ = nullptr;
  QPushButton* queueExportButton_ = nullptr;
  QPushButton* applyLayoutCurrentButton_ = nullptr;
  QPushButton* applyLayoutSelectedButton_ = nullptr;
  QLineEdit* sourceFilterEdit_ = nullptr;
  QTreeWidget* sourceTree_ = nullptr;
  QCheckBox* batchModeCheck_ = nullptr;
  QListWidget* queueListWidget_ = nullptr;
  QComboBox* queueLayoutCombo_ = nullptr;
  QLineEdit* queueExportDirectoryEdit_ = nullptr;
  QLineEdit* layoutNameEdit_ = nullptr;
  QSpinBox* layoutRowsSpin_ = nullptr;
  QSpinBox* layoutColumnsSpin_ = nullptr;
  QSpinBox* layoutGutterSpin_ = nullptr;
  QSpinBox* layoutMarginSpin_ = nullptr;
  QSpinBox* layoutSharpnessSpin_ = nullptr;
  QSpinBox* layoutRoundingSpin_ = nullptr;
  QSpinBox* layoutShadowSpin_ = nullptr;
  QSpinBox* layoutBorderSpin_ = nullptr;
  QCheckBox* layoutMetadataCheck_ = nullptr;
  QCheckBox* layoutTimestampsCheck_ = nullptr;
  QCheckBox* layoutDarkModeCheck_ = nullptr;
  QLineEdit* watermarkTextEdit_ = nullptr;
  QDoubleSpinBox* watermarkOpacitySpin_ = nullptr;
  QLineEdit* watermarkImagePathEdit_ = nullptr;
  QDoubleSpinBox* watermarkImageOpacitySpin_ = nullptr;
  QComboBox* exportFormatCombo_ = nullptr;
  QDoubleSpinBox* exportScaleSpin_ = nullptr;
  QListWidget* layoutPresetList_ = nullptr;
  QTableWidget* layoutTilesTable_ = nullptr;
  QTextEdit* statusText_ = nullptr;
  QDialog* devOverlay_ = nullptr;
  QAction* recentVideosMenuAction_ = nullptr;
  QAction* recentLayoutsMenuAction_ = nullptr;
  QMenu* recentVideosMenu_ = nullptr;
  QMenu* recentLayoutsMenu_ = nullptr;
  ProjectInfo projectInfo_;
  QString sheetPreviewPath_;
  QString layoutPreviewPath_;
  QString batchExportDirectory_;
  QStringList recentVideoPaths_;
  QStringList recentLayoutPaths_;
  int loadRequestId_ = 0;
  int previewRequestId_ = 0;
  int layoutPreviewRequestId_ = 0;
  int timelineStripRequestId_ = 0;
  qint64 rangeStartMs_ = 0;
  qint64 rangeEndMs_ = 0;
  qint64 samplingStartMs_ = 0;
  std::optional<QRectF> appliedCrop_;
  QVector<ReviewSourceState> sources_;
  QVector<LayoutPresetState> layoutPresets_;
  int activeSourceIndex_ = -1;
  int currentLayoutPresetIndex_ = -1;
  bool suppressLayoutEditorUpdates_ = false;
};
