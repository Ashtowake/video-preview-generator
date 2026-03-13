#include "MainWindow.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStackedLayout>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

#include <QtConcurrent>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>

#include "CropOverlayWidget.hpp"
#include "InteractiveSheetWidget.hpp"
#include "MpvWidget.hpp"
#include "TimelineWidget.hpp"

namespace {

constexpr int kReviewPreviewWidth = 1200;
constexpr int kLayoutPreviewWidth = 900;

struct BackgroundLoadResult {
  int requestId = 0;
  QString videoPath;
  ProjectInfo info;
  QString inspectError;
};

QString normalizePath(const QString& path)
{
  const QFileInfo info(path);
  return info.canonicalFilePath().isEmpty() ? info.absoluteFilePath() : info.canonicalFilePath();
}

double effectivePreviewAspectRatio(const ProjectInfo& info, const std::optional<QRectF>& crop)
{
  const double baseWidth = std::max(1, info.width);
  const double baseHeight = std::max(1, info.height);
  if (!crop.has_value()) {
    return baseWidth / baseHeight;
  }

  const double croppedWidth = std::max(1.0, crop->width() * baseWidth);
  const double croppedHeight = std::max(1.0, crop->height() * baseHeight);
  return croppedWidth / croppedHeight;
}

int timelineThumbnailCount(qint64 durationMs)
{
  const qint64 durationSeconds = std::max<qint64>(1, durationMs / 1000);
  return std::clamp<int>(static_cast<int>(durationSeconds / 10), 32, 120);
}

QString statusForSource(const ReviewSourceState& source)
{
  if (!source.lastError.isEmpty()) {
    return "Error";
  }
  if (source.loading) {
    return "Loading";
  }
  if (!source.info.valid) {
    return "Pending";
  }

  QStringList badges{ "Ready" };
  if (source.crop.has_value()) {
    badges << "Cropped";
  }
  if (source.rangeStartMs > 0 || (source.info.durationMs > 0 && source.rangeEndMs < source.info.durationMs)) {
    badges << "Ranged";
  }
  if (source.exported) {
    badges << "Exported";
  }
  return badges.join(" / ");
}

QString safeLayoutName(const LayoutPresetState& preset)
{
  return preset.name.isEmpty() ? QStringLiteral("Unnamed Layout") : preset.name;
}

QString sanitizedStem(const QString& input)
{
  QString stem = input;
  stem.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")), QStringLiteral("-"));
  stem = stem.trimmed();
  while (stem.contains(QStringLiteral("--"))) {
    stem.replace(QStringLiteral("--"), QStringLiteral("-"));
  }
  return stem.isEmpty() ? QStringLiteral("layout") : stem;
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
  : QMainWindow(parent)
{
  setWindowTitle("Video Preview Generator");
  resize(1720, 1040);
  setAcceptDrops(true);
  applyDarkPalette();

  layoutPresets_.append(defaultLayoutPreset());
  currentLayoutPresetIndex_ = 0;

  createUi();
  createMenuBar();
  restoreUiState();
  refreshLayoutPresetList();
  syncLayoutEditorsFromCurrentPreset();
  updateLayoutControlsEnabled();
  refreshQueuePanel();
  statusBar()->showMessage("Native shell ready");
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
  if (!event->mimeData()->hasUrls()) {
    event->ignore();
    return;
  }

  const QList<QUrl> urls = event->mimeData()->urls();
  const bool hasLocalFile = std::any_of(urls.begin(), urls.end(), [](const QUrl& url) {
    return url.isLocalFile();
  });

  if (!hasLocalFile) {
    event->ignore();
    return;
  }

  event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event)
{
  if (!event->mimeData()->hasUrls()) {
    event->ignore();
    return;
  }

  QStringList paths;
  for (const QUrl& url : event->mimeData()->urls()) {
    if (url.isLocalFile()) {
      const QString path = url.toLocalFile();
      if (!path.isEmpty()) {
        paths.append(path);
      }
    }
  }

  if (paths.isEmpty()) {
    event->ignore();
    return;
  }

  addSources(paths, true);
  event->acceptProposedAction();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
  saveUiState();
  QMainWindow::closeEvent(event);
}

void MainWindow::addVideos()
{
  const QStringList paths = QFileDialog::getOpenFileNames(
    this,
    tr("Add Videos"),
    QString(),
    tr("Video Files (*.mp4 *.mkv *.mov *.avi *.webm *.m4v);;All Files (*)"));
  if (paths.isEmpty()) {
    return;
  }

  addSources(paths, true);
}

void MainWindow::addSources(const QStringList& paths, bool activateLast)
{
  int lastAddedIndex = -1;
  const LayoutPresetState preset = currentLayoutPreset() ? *currentLayoutPreset() : defaultLayoutPreset();

  for (const QString& rawPath : paths) {
    const QString path = normalizePath(rawPath);
    if (path.isEmpty()) {
      continue;
    }

    const int existingIndex = findSourceIndex(path);
    if (existingIndex >= 0) {
      lastAddedIndex = existingIndex;
      continue;
    }

    const QFileInfo fileInfo(path);
    ReviewSourceState source;
    source.path = path;
    source.displayName = fileInfo.fileName();
    source.loading = true;
    source.appliedLayoutPresetId = preset.id;
    source.rangeStartMs = 0;
    source.rangeEndMs = 0;
    source.samplingStartMs = 0;
    applyLayoutToSourceState(source, preset, true);
    sources_.append(source);
    lastAddedIndex = sources_.size() - 1;
    updateRecentVideos(path);
  }

  refreshSourceBin();

  if (activateLast && lastAddedIndex >= 0) {
    activateSourceByIndex(lastAddedIndex);
  } else {
    refreshQueuePanel();
  }
}

void MainWindow::loadVideo(const QString& path)
{
  addSources({ path }, true);
}

void MainWindow::activateSourceByIndex(int index)
{
  if (index < 0 || index >= sources_.size()) {
    return;
  }

  activeSourceIndex_ = index;
  ReviewSourceState& source = sources_[index];
  projectInfo_ = source.info;
  rangeStartMs_ = source.rangeStartMs;
  rangeEndMs_ = source.rangeEndMs;
  samplingStartMs_ = source.samplingStartMs;
  appliedCrop_ = source.crop;

  titleLabel_->setText(source.displayName.isEmpty() ? QStringLiteral("No source selected") : source.displayName);
  infoLabel_->setText(source.info.valid
    ? QStringLiteral("%1 ms  |  %2x%3  |  %4 fps  |  %5 frames")
        .arg(source.info.durationMs)
        .arg(source.info.width)
        .arg(source.info.height)
        .arg(source.info.fps, 0, 'f', 2)
        .arg(source.info.frameCount)
    : QStringLiteral("Loading metadata in the background..."));
  infoLabel_->setToolTip(source.path);

  cropOverlay_->setAppliedCrop(source.crop);
  cropOverlay_->clearPendingCrop();
  cropOverlay_->setSourceVideoSize(QSize(source.info.width, source.info.height));
  updateCropUi();

  if (source.info.valid) {
    timelineWidget_->setFramesPerSecond(source.info.fps);
    timelineWidget_->setDurationMs(source.info.durationMs);
    updateSelectedRange(source.rangeStartMs, source.rangeEndMs, false);
    updateSamplingStart(source.samplingStartMs, false);
    updateTransport(source.rangeStartMs, source.info.durationMs);
    const LayoutPresetState* appliedPreset = appliedLayoutForSource(source);
    reviewSheetWidget_->setLayoutState(sheetLayoutPreviewState(appliedPreset ? *appliedPreset : defaultLayoutPreset(), &source));
    reviewSheetWidget_->setSelectedTileId(source.selectedTileId);
    mpvWidget_->loadFile(source.path);
    beginTimelineStripRender();
    beginPreviewRender();
  } else {
    timelineWidget_->setDurationMs(0);
    timelineWidget_->setFilmstrip(QPixmap(), 0, 16.0 / 9.0);
    timelineWidget_->setFilmstripLoading(true);
    reviewSheetWidget_->clearPreview("Loading source metadata...");
    const LayoutPresetState* appliedPreset = appliedLayoutForSource(source);
    reviewSheetWidget_->setLayoutState(sheetLayoutPreviewState(appliedPreset ? *appliedPreset : defaultLayoutPreset(), &source));
    mpvWidget_->loadFile(source.path);
    beginBackgroundLoad(index);
  }

  refreshSourceBin();
  refreshQueuePanel();
  updateQuickSheetControls();
  refreshLayoutPreview();
}

void MainWindow::activateCurrentSourceFromSelection()
{
  if (!sourceTree_->currentItem()) {
    return;
  }

  const QString path = sourceTree_->currentItem()->data(0, Qt::UserRole).toString();
  const int index = findSourceIndex(path);
  if (index >= 0 && index != activeSourceIndex_) {
    activateSourceByIndex(index);
  }
}

void MainWindow::beginBackgroundLoad(int sourceIndex)
{
  if (sourceIndex < 0 || sourceIndex >= sources_.size()) {
    return;
  }

  loadRequestId_ += 1;
  const int requestId = loadRequestId_;
  sources_[sourceIndex].loading = true;
  refreshSourceBin();

  const QString path = sources_[sourceIndex].path;
  auto* watcher = new QFutureWatcher<BackgroundLoadResult>(this);
  connect(watcher, &QFutureWatcher<BackgroundLoadResult>::finished, this, [this, watcher] {
    const BackgroundLoadResult result = watcher->result();
    watcher->deleteLater();

    if (result.requestId != loadRequestId_) {
      return;
    }

    const int index = findSourceIndex(result.videoPath);
    if (index < 0) {
      return;
    }

    ReviewSourceState& source = sources_[index];
    source.loading = false;
    if (result.info.valid) {
      source.info = result.info;
      source.lastError.clear();
      if (source.rangeEndMs <= source.rangeStartMs) {
        source.rangeStartMs = 0;
        source.rangeEndMs = result.info.durationMs;
        source.samplingStartMs = source.rangeStartMs;
      }
      const LayoutPresetState preset = currentLayoutPreset() ? *currentLayoutPreset() : defaultLayoutPreset();
      applyLayoutToSourceState(source, preset, true);
      appendStatusMessage(QStringLiteral("Loaded %1 via Rust CLI metadata bridge.").arg(result.info.displayName));
      if (index == activeSourceIndex_) {
        updateMetadata(result.info);
        beginTimelineStripRender();
        beginPreviewRender();
      }
    } else {
      source.lastError = result.inspectError;
      if (index == activeSourceIndex_) {
        infoLabel_->setText("Metadata probe failed.");
        timelineWidget_->setFilmstripLoading(false);
        reviewSheetWidget_->clearPreview("Failed to inspect video metadata.");
        appendStatusMessage(result.inspectError);
      }
    }

    refreshSourceBin();
    refreshQueuePanel();
    refreshLayoutPreview();
  });

  watcher->setFuture(QtConcurrent::run([path, requestId]() {
    BackgroundLoadResult result;
    result.requestId = requestId;
    result.videoPath = path;
    RustBridge bridge;
    result.info = bridge.inspectVideo(path, &result.inspectError);
    return result;
  }));
}

void MainWindow::beginPreviewRender()
{
  const ReviewSourceState* source = activeSource();
  const LayoutPresetState* preset = source ? appliedLayoutForSource(*source) : nullptr;
  if (!source || !preset || !source->info.valid) {
    return;
  }

  previewRequestId_ += 1;
  const int requestId = previewRequestId_;
  const QJsonObject project = buildProjectJson(*source, *preset);
  reviewSheetWidget_->clearPreview("Rendering interactive sheet preview in the background...");
  reviewSheetWidget_->setLayoutState(sheetLayoutPreviewState(*preset, source));
  reviewSheetWidget_->setSelectedTileId(source->selectedTileId);

  auto* watcher = new QFutureWatcher<QPair<QString, QString>>(this);
  connect(watcher, &QFutureWatcher<QPair<QString, QString>>::finished, this, [this, watcher, requestId] {
    const auto [previewPath, previewError] = watcher->result();
    watcher->deleteLater();

    if (requestId != previewRequestId_) {
      return;
    }

    if (!previewPath.isEmpty()) {
      showSheetPreview(previewPath);
      appendStatusMessage(QStringLiteral("Rendered sheet preview to %1").arg(previewPath));
      return;
    }

    reviewSheetWidget_->clearPreview(previewError.isEmpty()
      ? "Failed to render sheet preview."
      : previewError);
    appendStatusMessage(previewError.isEmpty()
      ? "Failed to render sheet preview."
      : previewError);
  });

  watcher->setFuture(QtConcurrent::run([project]() {
    QString errorMessage;
    RustBridge bridge;
    const QString previewPath = bridge.renderProjectPreview(project, &errorMessage, kReviewPreviewWidth);
    return qMakePair(previewPath, errorMessage);
  }));
}

void MainWindow::beginLayoutPreviewRender()
{
  const LayoutPresetState* preset = currentLayoutPreset();
  if (!preset) {
    layoutPreviewWidget_->clearPreview("No layout preset selected.");
    return;
  }

  const ReviewSourceState* source = activeSource();
  if (!source || !source->info.valid) {
    layoutPreviewWidget_->clearPreview("Load a source to preview this layout against real frames.");
    layoutPreviewWidget_->setLayoutState(sheetLayoutPreviewState(*preset, nullptr));
    return;
  }

  layoutPreviewRequestId_ += 1;
  const int requestId = layoutPreviewRequestId_;
  ReviewSourceState previewSource = *source;
  applyLayoutToSourceState(previewSource, *preset, true);
  const QJsonObject project = buildProjectJson(previewSource, *preset);

  layoutPreviewWidget_->clearPreview("Rendering layout preview in the background...");
  layoutPreviewWidget_->setLayoutState(sheetLayoutPreviewState(*preset, &previewSource));
  layoutPreviewWidget_->setSelectedTileId(previewSource.selectedTileId);

  auto* watcher = new QFutureWatcher<QPair<QString, QString>>(this);
  connect(watcher, &QFutureWatcher<QPair<QString, QString>>::finished, this, [this, watcher, requestId] {
    const auto [previewPath, previewError] = watcher->result();
    watcher->deleteLater();

    if (requestId != layoutPreviewRequestId_) {
      return;
    }

    if (!previewPath.isEmpty()) {
      showLayoutPreview(previewPath);
      return;
    }

    layoutPreviewWidget_->clearPreview(previewError.isEmpty()
      ? "Failed to render layout preview."
      : previewError);
  });

  watcher->setFuture(QtConcurrent::run([project]() {
    QString errorMessage;
    RustBridge bridge;
    const QString previewPath = bridge.renderProjectPreview(project, &errorMessage, kLayoutPreviewWidth);
    return qMakePair(previewPath, errorMessage);
  }));
}

void MainWindow::beginTimelineStripRender()
{
  const ReviewSourceState* source = activeSource();
  if (!source || !source->info.valid || source->path.isEmpty()) {
    return;
  }

  timelineWidget_->setFilmstripLoading(true);
  timelineStripRequestId_ += 1;
  const int requestId = timelineStripRequestId_;
  const QString videoPath = source->path;
  const std::optional<QRectF> crop = source->crop;
  const QSize stripSize = timelineWidget_->filmstripImageSize().expandedTo(QSize(960, 84));
  const int thumbnailCount = timelineThumbnailCount(source->info.durationMs);
  const int targetWidth = thumbnailCount * 72;
  const double frameAspectRatio = effectivePreviewAspectRatio(source->info, crop);

  auto* watcher = new QFutureWatcher<QPair<QString, QString>>(this);
  connect(watcher, &QFutureWatcher<QPair<QString, QString>>::finished, this, [this, watcher, requestId, thumbnailCount, frameAspectRatio] {
    const auto [stripPath, stripError] = watcher->result();
    watcher->deleteLater();

    if (requestId != timelineStripRequestId_) {
      return;
    }

    if (stripPath.isEmpty()) {
      timelineWidget_->setFilmstrip(QPixmap(), 0, frameAspectRatio);
      timelineWidget_->setFilmstripLoading(false);
      appendStatusMessage(stripError.isEmpty() ? "Failed to render timeline thumbnails." : stripError);
      return;
    }

    QPixmap filmstrip(stripPath);
    if (filmstrip.isNull()) {
      timelineWidget_->setFilmstrip(QPixmap(), 0, frameAspectRatio);
      timelineWidget_->setFilmstripLoading(false);
      appendStatusMessage(QStringLiteral("Failed to load timeline strip image %1").arg(stripPath));
      return;
    }

    timelineWidget_->setFilmstrip(filmstrip, thumbnailCount, frameAspectRatio);
    timelineWidget_->setFilmstripLoading(false);
  });

  watcher->setFuture(QtConcurrent::run([videoPath, crop, stripSize, thumbnailCount, targetWidth]() {
    QString errorMessage;
    RustBridge bridge;
    const QString stripPath = bridge.renderTimelineStrip(
      videoPath,
      &errorMessage,
      thumbnailCount,
      targetWidth,
      stripSize.height(),
      crop);
    return qMakePair(stripPath, errorMessage);
  }));
}

void MainWindow::beginCropSelection()
{
  if (!projectInfo_.valid || projectInfo_.width <= 0 || projectInfo_.height <= 0) {
    appendStatusMessage("Crop selection is not available until video metadata is loaded.");
    return;
  }

  cropOverlay_->beginSelection();
  statusBar()->showMessage("Drag over the player to choose a crop area.", 4000);
  appendStatusMessage("Crop selection started.");
}

void MainWindow::applyPendingCrop()
{
  ReviewSourceState* source = activeSource();
  const std::optional<QRectF> pendingCrop = cropOverlay_->pendingCrop();
  if (!source || !pendingCrop.has_value()) {
    return;
  }

  source->crop = pendingCrop;
  appliedCrop_ = pendingCrop;
  cropOverlay_->setAppliedCrop(appliedCrop_);
  updateCropUi();
  beginTimelineStripRender();
  refreshSheetPreview();
  refreshLayoutPreview();
  refreshSourceBin();
  appendStatusMessage(QStringLiteral(
    "Applied crop x=%1 y=%2 width=%3 height=%4")
    .arg(appliedCrop_->x(), 0, 'f', 3)
    .arg(appliedCrop_->y(), 0, 'f', 3)
    .arg(appliedCrop_->width(), 0, 'f', 3)
    .arg(appliedCrop_->height(), 0, 'f', 3));
}

void MainWindow::clearCrop()
{
  ReviewSourceState* source = activeSource();
  if (!source) {
    return;
  }

  source->crop.reset();
  appliedCrop_.reset();
  cropOverlay_->setAppliedCrop(std::nullopt);
  cropOverlay_->clearPendingCrop();
  updateCropUi();
  beginTimelineStripRender();
  refreshSheetPreview();
  refreshLayoutPreview();
  refreshSourceBin();
  appendStatusMessage("Cleared crop.");
}

void MainWindow::updateCropUi()
{
  const ReviewSourceState* source = activeSource();
  const bool hasPendingCrop = cropOverlay_->pendingCrop().has_value();
  const bool hasAppliedCrop = source && source->crop.has_value();
  const bool canSelectCrop = source && source->info.valid && source->info.width > 0 && source->info.height > 0;

  selectCropButton_->setEnabled(canSelectCrop);
  applyCropButton_->setEnabled(hasPendingCrop);
  clearCropButton_->setEnabled(hasPendingCrop || hasAppliedCrop);

  if (hasPendingCrop) {
    const QRectF crop = *cropOverlay_->pendingCrop();
    cropStatusLabel_->setText(QStringLiteral(
      "Pending crop %1% x %2%")
      .arg(crop.width() * 100.0, 0, 'f', 1)
      .arg(crop.height() * 100.0, 0, 'f', 1));
    return;
  }

  if (hasAppliedCrop) {
    const QRectF crop = *source->crop;
    cropStatusLabel_->setText(QStringLiteral(
      "Applied crop %1% x %2%")
      .arg(crop.width() * 100.0, 0, 'f', 1)
      .arg(crop.height() * 100.0, 0, 'f', 1));
    return;
  }

  cropStatusLabel_->setText("No crop");
}

void MainWindow::toggleDevOverlay()
{
  if (!devOverlay_) {
    return;
  }

  if (devOverlay_->isVisible()) {
    devOverlay_->hide();
    return;
  }

  const QPoint overlayOffset = QPoint(width() - devOverlay_->width() - 24, 48);
  devOverlay_->move(mapToGlobal(overlayOffset));
  devOverlay_->show();
  devOverlay_->raise();
  devOverlay_->activateWindow();
}

void MainWindow::updateMetadata(const ProjectInfo& info)
{
  ReviewSourceState* source = activeSource();
  if (!source) {
    return;
  }

  source->info = info;
  source->loading = false;
  source->lastError.clear();
  if (source->rangeEndMs <= source->rangeStartMs) {
    source->rangeStartMs = 0;
    source->rangeEndMs = info.durationMs;
    source->samplingStartMs = source->rangeStartMs;
  }

  projectInfo_ = info;
  titleLabel_->setText(info.displayName.isEmpty() ? "No video loaded" : info.displayName);
  infoLabel_->setText(QStringLiteral("%1 ms  |  %2x%3  |  %4 fps  |  %5 frames")
    .arg(info.durationMs)
    .arg(info.width)
    .arg(info.height)
    .arg(info.fps, 0, 'f', 2)
    .arg(info.frameCount));
  infoLabel_->setToolTip(info.videoPath);
  backendLabel_->setText(QStringLiteral("Rust core bridge: %1")
    .arg(rustBridge_.cliPath().isEmpty() ? "unavailable" : rustBridge_.cliPath()));

  timelineWidget_->setFramesPerSecond(info.fps);
  timelineWidget_->setDurationMs(info.durationMs);
  cropOverlay_->setSourceVideoSize(QSize(info.width, info.height));

  rangeStartMs_ = source->rangeStartMs;
  rangeEndMs_ = source->rangeEndMs;
  samplingStartMs_ = source->samplingStartMs;
  appliedCrop_ = source->crop;
  updateSelectedRange(source->rangeStartMs, source->rangeEndMs, false);
  updateSamplingStart(source->samplingStartMs, false);
  updateCropUi();
  updateTransport(source->rangeStartMs, info.durationMs);
  const LayoutPresetState* appliedPreset = appliedLayoutForSource(*source);
  reviewSheetWidget_->setLayoutState(sheetLayoutPreviewState(appliedPreset ? *appliedPreset : defaultLayoutPreset(), source));
  updateQuickSheetControls();
}

void MainWindow::updateTransport(qint64 positionMs, qint64 durationMs)
{
  if (!mpvWidget_->isPaused()
    && rangeEndMs_ > rangeStartMs_
    && durationMs > 0
    && positionMs >= rangeEndMs_) {
    mpvWidget_->pause();
    mpvWidget_->seekAbsoluteMs(rangeEndMs_);
    return;
  }

  timelineWidget_->setDurationMs(durationMs);
  if (!timelineWidget_->isScrubbing()) {
    timelineWidget_->setPositionMs(positionMs);
  }

  timeLabel_->setText(QStringLiteral("%1 / %2").arg(formatTime(positionMs), formatTime(durationMs)));
  frameLabel_->setText(QStringLiteral("Frame %1").arg(displayFrameNumber(positionMs)));
}

void MainWindow::updateSelectedRange(qint64 startMs, qint64 endMs, bool refreshPreview)
{
  ReviewSourceState* source = activeSource();
  if (!source) {
    return;
  }

  source->rangeStartMs = qMax<qint64>(0, startMs);
  source->rangeEndMs = qMax(source->rangeStartMs, endMs);
  rangeStartMs_ = source->rangeStartMs;
  rangeEndMs_ = source->rangeEndMs;
  timelineWidget_->setSelectionRangeMs(rangeStartMs_, rangeEndMs_);
  rangeLabel_->setText(
    rangeEndMs_ > 0
      ? QStringLiteral("Range %1 - %2").arg(formatTime(rangeStartMs_), formatTime(rangeEndMs_))
      : QStringLiteral("Range 0:00.000 - 0:00.000"));

  if (source->samplingStartMs < source->rangeStartMs || source->samplingStartMs > source->rangeEndMs) {
    source->samplingStartMs = source->rangeStartMs;
  }
  reassignAutoTiles(*source);
  updateQuickSheetControls();
  refreshSourceBin();

  if (refreshPreview && source->info.valid) {
    refreshSheetPreview();
  }
}

void MainWindow::updateSamplingStart(qint64 samplingStartMs, bool refreshPreview)
{
  ReviewSourceState* source = activeSource();
  if (!source) {
    return;
  }

  source->samplingStartMs = std::clamp<qint64>(samplingStartMs, source->rangeStartMs, source->rangeEndMs);
  samplingStartMs_ = source->samplingStartMs;
  timelineWidget_->setSamplingStartMs(samplingStartMs_);
  samplingStartLabel_->setText(QStringLiteral("Sampling %1").arg(formatTime(samplingStartMs_)));
  reassignAutoTiles(*source);
  updateQuickSheetControls();
  refreshSourceBin();

  if (refreshPreview && source->info.valid) {
    refreshSheetPreview();
  }
}

void MainWindow::refreshSheetPreview()
{
  beginPreviewRender();
}

void MainWindow::refreshLayoutPreview()
{
  beginLayoutPreviewRender();
}

void MainWindow::showSheetPreview(const QString& imagePath)
{
  sheetPreviewPath_ = imagePath;
  QPixmap pixmap(imagePath);
  if (pixmap.isNull()) {
    reviewSheetWidget_->clearPreview(QStringLiteral("Failed to load rendered preview image %1").arg(imagePath));
    return;
  }

  reviewSheetWidget_->setPreviewImage(pixmap);
}

void MainWindow::showLayoutPreview(const QString& imagePath)
{
  layoutPreviewPath_ = imagePath;
  QPixmap pixmap(imagePath);
  if (pixmap.isNull()) {
    layoutPreviewWidget_->clearPreview(QStringLiteral("Failed to load rendered preview image %1").arg(imagePath));
    return;
  }

  layoutPreviewWidget_->setPreviewImage(pixmap);
}

void MainWindow::playSelectedRange()
{
  if (!projectInfo_.valid) {
    return;
  }

  const qint64 targetStartMs = qMax<qint64>(0, rangeStartMs_);
  const qint64 currentTimeMs = mpvWidget_->currentTimeMs();
  const bool outsideSelectedRange = currentTimeMs < targetStartMs
    || (rangeEndMs_ > targetStartMs && currentTimeMs >= rangeEndMs_);

  if (outsideSelectedRange) {
    mpvWidget_->seekAbsoluteMs(targetStartMs);
  }

  mpvWidget_->play();
}

void MainWindow::togglePlayback()
{
  if (!projectInfo_.valid) {
    return;
  }

  if (mpvWidget_->isPaused()) {
    playSelectedRange();
    return;
  }

  mpvWidget_->pause();
}

void MainWindow::stopSelectedRange()
{
  mpvWidget_->pause();
  mpvWidget_->seekAbsoluteMs(qMax<qint64>(0, rangeStartMs_));
}

void MainWindow::assignCurrentFrameToSelectedTile()
{
  ReviewSourceState* source = activeSource();
  if (!source || source->selectedTileId.isEmpty()) {
    return;
  }

  for (ReviewTileState& tile : source->tiles) {
    if (tile.layout.id != source->selectedTileId) {
      continue;
    }

    tile.frameIndex = frameIndexAtTime(mpvWidget_->currentTimeMs(), source->info.fps, source->info.frameCount);
    tile.timeMs = seekTimeForFrameIndex(tile.frameIndex, source->info.durationMs, source->info.fps);
    tile.fineTuneOffsetMs = 0;
    tile.pinned = true;
    appendStatusMessage(QStringLiteral("Assigned %1 to %2")
      .arg(formatTime(tile.timeMs), tile.layout.id));
    break;
  }

  updateQuickSheetControls();
  refreshSheetPreview();
  refreshSourceBin();
}

void MainWindow::fineTuneSelectedTileFrames(int direction)
{
  ReviewSourceState* source = activeSource();
  if (!source || source->selectedTileId.isEmpty()) {
    return;
  }

  for (ReviewTileState& tile : source->tiles) {
    if (tile.layout.id != source->selectedTileId) {
      continue;
    }

    const qint64 nextFrame = std::max<qint64>(0, static_cast<qint64>(tile.frameIndex) + direction);
    tile.frameIndex = static_cast<qint64>(std::min<qint64>(nextFrame, std::max<qint64>(0, source->info.frameCount - 1)));
    tile.timeMs = seekTimeForFrameIndex(tile.frameIndex, source->info.durationMs, source->info.fps);
    tile.fineTuneOffsetMs = 0;
    tile.pinned = true;
    mpvWidget_->seekAbsoluteMs(tile.timeMs);
    appendStatusMessage(QStringLiteral("Fine-tuned %1 to frame %2")
      .arg(tile.layout.id)
      .arg(tile.frameIndex + 1));
    break;
  }

  updateQuickSheetControls();
  refreshSheetPreview();
  refreshSourceBin();
}

void MainWindow::toggleSelectedTilePinned()
{
  ReviewSourceState* source = activeSource();
  if (!source || source->selectedTileId.isEmpty()) {
    return;
  }

  for (ReviewTileState& tile : source->tiles) {
    if (tile.layout.id != source->selectedTileId) {
      continue;
    }

    tile.pinned = !tile.pinned;
    if (!tile.pinned) {
      reassignAutoTiles(*source);
    }
    appendStatusMessage(QStringLiteral("%1 %2")
      .arg(tile.pinned ? "Pinned" : "Unpinned", tile.layout.id));
    break;
  }

  updateQuickSheetControls();
  refreshSheetPreview();
  refreshSourceBin();
}

void MainWindow::findSharpestForSelectedTile()
{
  ReviewSourceState* source = activeSource();
  const LayoutPresetState* preset = currentLayoutPreset();
  if (!source || !preset || source->selectedTileId.isEmpty()) {
    return;
  }

  const QJsonObject project = buildProjectJson(*source, *preset);
  const QJsonObject updated = rustBridge_.findSharpestNeighbours(project, { source->selectedTileId }, nullptr);
  if (updated.isEmpty()) {
    appendStatusMessage(QStringLiteral("Failed to update %1 with sharpest-neighbour search.").arg(source->selectedTileId));
    return;
  }

  updateSourceFromProjectJson(*source, updated);
  updateQuickSheetControls();
  refreshSheetPreview();
  appendStatusMessage(QStringLiteral("Updated %1 from sharpest-neighbour search.").arg(source->selectedTileId));
}

void MainWindow::applyLayoutToCurrentSource()
{
  ReviewSourceState* source = activeSource();
  const LayoutPresetState* preset = currentLayoutPreset();
  if (!source || !preset) {
    return;
  }

  applyLayoutToSourceState(*source, *preset, true);
  reviewSheetWidget_->setLayoutState(sheetLayoutPreviewState(*preset, source));
  reviewSheetWidget_->setSelectedTileId(source->selectedTileId);
  refreshSheetPreview();
  refreshSourceBin();
  refreshQueuePanel();
  appendStatusMessage(QStringLiteral("Applied layout %1 to %2").arg(safeLayoutName(*preset), source->displayName));
}

void MainWindow::applyLayoutToSelectedSources()
{
  const LayoutPresetState* preset = currentLayoutPreset();
  if (!preset) {
    return;
  }

  const QList<QTreeWidgetItem*> items = sourceTree_->selectedItems();
  if (items.isEmpty()) {
    applyLayoutToCurrentSource();
    return;
  }

  for (QTreeWidgetItem* item : items) {
    const int index = findSourceIndex(item->data(0, Qt::UserRole).toString());
    if (index < 0) {
      continue;
    }
    applyLayoutToSourceState(sources_[index], *preset, true);
  }

  refreshSourceBin();
  refreshQueuePanel();
  if (activeSource()) {
    reviewSheetWidget_->setLayoutState(sheetLayoutPreviewState(*preset, activeSource()));
    refreshSheetPreview();
  }
  appendStatusMessage(QStringLiteral("Applied layout %1 to %2 selected sources")
    .arg(safeLayoutName(*preset))
    .arg(items.size()));
}

void MainWindow::exportBatchQueue()
{
  if (batchExportDirectory_.isEmpty()) {
    chooseBatchExportDirectory();
    if (batchExportDirectory_.isEmpty()) {
      return;
    }
  }

  const int presetIndex = queueLayoutCombo_->currentIndex();
  if (presetIndex < 0 || presetIndex >= layoutPresets_.size()) {
    appendStatusMessage("No layout preset selected for export.");
    return;
  }

  const LayoutPresetState preset = layoutPresets_[presetIndex];
  const QList<QTreeWidgetItem*> items = sourceTree_->selectedItems();
  if (items.isEmpty()) {
    appendStatusMessage("Select at least one source in batch mode before exporting.");
    return;
  }

  int successCount = 0;
  for (QTreeWidgetItem* item : items) {
    const int index = findSourceIndex(item->data(0, Qt::UserRole).toString());
    if (index < 0) {
      continue;
    }

    ReviewSourceState exportSource = sources_[index];
    applyLayoutToSourceState(exportSource, preset, true);
    const QString baseName = QFileInfo(exportSource.path).completeBaseName();
    const QString extension = preset.exportFormat.compare("jpeg", Qt::CaseInsensitive) == 0 ? "jpg" : "png";
    const QString outputPath = QDir(batchExportDirectory_).filePath(
      QStringLiteral("%1-%2.%3").arg(sanitizedStem(baseName), sanitizedStem(safeLayoutName(preset)), extension));
    QString errorMessage;
    const QString resultPath = rustBridge_.exportProject(buildProjectJson(exportSource, preset), outputPath, &errorMessage);
    if (resultPath.isEmpty()) {
      sources_[index].lastError = errorMessage.isEmpty() ? "Export failed." : errorMessage;
      appendStatusMessage(QStringLiteral("Failed to export %1: %2").arg(exportSource.displayName, sources_[index].lastError));
      continue;
    }

    sources_[index].exported = true;
    sources_[index].lastExportPath = resultPath;
    sources_[index].lastError.clear();
    successCount += 1;
  }

  refreshSourceBin();
  refreshQueuePanel();
  appendStatusMessage(QStringLiteral("Exported %1 source(s) to %2").arg(successCount).arg(batchExportDirectory_));
}

void MainWindow::chooseBatchExportDirectory()
{
  const QString directory = QFileDialog::getExistingDirectory(
    this,
    tr("Choose Export Directory"),
    batchExportDirectory_.isEmpty() ? QDir::homePath() : batchExportDirectory_);
  if (directory.isEmpty()) {
    return;
  }

  batchExportDirectory_ = directory;
  queueExportDirectoryEdit_->setText(directory);
}

void MainWindow::chooseWatermarkImage()
{
  const QString path = QFileDialog::getOpenFileName(
    this,
    tr("Choose Watermark Image"),
    QString(),
    tr("Images (*.png *.jpg *.jpeg *.webp *.bmp);;All Files (*)"));
  if (path.isEmpty()) {
    return;
  }

  watermarkImagePathEdit_->setText(path);
  syncCurrentPresetFromEditors();
}

void MainWindow::createNewLayoutPreset()
{
  LayoutPresetState preset = defaultLayoutPreset();
  preset.id = QStringLiteral("layout-%1").arg(layoutPresets_.size() + 1);
  preset.name = QStringLiteral("Layout %1").arg(layoutPresets_.size() + 1);
  layoutPresets_.append(preset);
  currentLayoutPresetIndex_ = layoutPresets_.size() - 1;
  refreshLayoutPresetList();
  syncLayoutEditorsFromCurrentPreset();
  refreshLayoutPreview();
}

void MainWindow::duplicateCurrentLayoutPreset()
{
  const LayoutPresetState* preset = currentLayoutPreset();
  if (!preset) {
    return;
  }

  LayoutPresetState duplicate = *preset;
  duplicate.id = QStringLiteral("%1-copy-%2").arg(preset->id).arg(layoutPresets_.size() + 1);
  duplicate.name = QStringLiteral("%1 Copy").arg(safeLayoutName(*preset));
  duplicate.filePath.clear();
  layoutPresets_.append(duplicate);
  currentLayoutPresetIndex_ = layoutPresets_.size() - 1;
  refreshLayoutPresetList();
  syncLayoutEditorsFromCurrentPreset();
  refreshLayoutPreview();
}

void MainWindow::renameCurrentLayoutPreset()
{
  LayoutPresetState* preset = currentLayoutPreset();
  if (!preset) {
    return;
  }

  bool accepted = false;
  const QString name = QInputDialog::getText(this, tr("Rename Layout"), tr("Layout name"), QLineEdit::Normal, preset->name, &accepted);
  if (!accepted || name.trimmed().isEmpty()) {
    return;
  }

  preset->name = name.trimmed();
  refreshLayoutPresetList();
  syncLayoutEditorsFromCurrentPreset();
  refreshQueuePanel();
}

void MainWindow::deleteCurrentLayoutPreset()
{
  if (layoutPresets_.size() <= 1 || currentLayoutPresetIndex_ < 0 || currentLayoutPresetIndex_ >= layoutPresets_.size()) {
    return;
  }

  const QString name = safeLayoutName(layoutPresets_[currentLayoutPresetIndex_]);
  if (QMessageBox::question(this, tr("Delete Layout"), tr("Delete layout \"%1\"?").arg(name))
      != QMessageBox::Yes) {
    return;
  }

  const QString deletedId = layoutPresets_[currentLayoutPresetIndex_].id;
  layoutPresets_.removeAt(currentLayoutPresetIndex_);
  currentLayoutPresetIndex_ = std::clamp(currentLayoutPresetIndex_, 0, static_cast<int>(layoutPresets_.size()) - 1);
  const QString fallbackId = layoutPresets_[currentLayoutPresetIndex_].id;
  for (ReviewSourceState& source : sources_) {
    if (source.appliedLayoutPresetId == deletedId) {
      applyLayoutToSourceState(source, layoutPresets_[currentLayoutPresetIndex_], true);
      source.appliedLayoutPresetId = fallbackId;
    }
  }
  refreshLayoutPresetList();
  syncLayoutEditorsFromCurrentPreset();
  refreshSourceBin();
  refreshQueuePanel();
  refreshSheetPreview();
  refreshLayoutPreview();
}

void MainWindow::loadLayoutPresetFromFile()
{
  const QString path = QFileDialog::getOpenFileName(
    this,
    tr("Load Layout Preset"),
    QString(),
    tr("Video Preview Layout (*.vpg-layout.json);;JSON Files (*.json)"));
  if (path.isEmpty()) {
    return;
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(this, tr("Load Layout"), tr("Failed to open %1").arg(path));
    return;
  }

  const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
  if (!document.isObject()) {
    QMessageBox::warning(this, tr("Load Layout"), tr("Layout file did not contain a JSON object."));
    return;
  }

  LayoutPresetState preset = parseLayoutPresetJson(document.object());
  preset.filePath = path;
  if (preset.id.isEmpty()) {
    preset.id = QStringLiteral("layout-%1").arg(layoutPresets_.size() + 1);
  }
  layoutPresets_.append(preset);
  currentLayoutPresetIndex_ = layoutPresets_.size() - 1;
  refreshLayoutPresetList();
  syncLayoutEditorsFromCurrentPreset();
  refreshLayoutPreview();
  updateRecentLayouts(path);
}

void MainWindow::saveCurrentLayoutPreset()
{
  LayoutPresetState* preset = currentLayoutPreset();
  if (!preset) {
    return;
  }

  if (preset->filePath.isEmpty()) {
    saveCurrentLayoutPresetAs();
    return;
  }

  QFile file(preset->filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    QMessageBox::warning(this, tr("Save Layout"), tr("Failed to write %1").arg(preset->filePath));
    return;
  }

  file.write(QJsonDocument(buildLayoutPresetJson(*preset)).toJson(QJsonDocument::Indented));
  file.close();
  updateRecentLayouts(preset->filePath);
  appendStatusMessage(QStringLiteral("Saved layout preset to %1").arg(preset->filePath));
}

void MainWindow::saveCurrentLayoutPresetAs()
{
  LayoutPresetState* preset = currentLayoutPreset();
  if (!preset) {
    return;
  }

  const QString path = QFileDialog::getSaveFileName(
    this,
    tr("Save Layout Preset"),
    preset->filePath.isEmpty() ? safeLayoutName(*preset) + ".vpg-layout.json" : preset->filePath,
    tr("Video Preview Layout (*.vpg-layout.json);;JSON Files (*.json)"));
  if (path.isEmpty()) {
    return;
  }

  preset->filePath = path;
  saveCurrentLayoutPreset();
}

void MainWindow::selectLayoutPreset(int index)
{
  if (index < 0 || index >= layoutPresets_.size()) {
    return;
  }

  currentLayoutPresetIndex_ = index;
  syncLayoutEditorsFromCurrentPreset();
  refreshLayoutPreview();
  refreshQueuePanel();
  updateLayoutControlsEnabled();
}

void MainWindow::syncLayoutEditorsFromCurrentPreset()
{
  if (currentLayoutPresetIndex_ < 0 || currentLayoutPresetIndex_ >= layoutPresets_.size()) {
    return;
  }

  suppressLayoutEditorUpdates_ = true;
  const LayoutPresetState& preset = layoutPresets_[currentLayoutPresetIndex_];
  layoutNameEdit_->setText(preset.name);
  layoutRowsSpin_->setValue(preset.rows);
  layoutColumnsSpin_->setValue(preset.columns);
  layoutGutterSpin_->setValue(preset.gutterPx);
  layoutMarginSpin_->setValue(preset.outerMarginPx);
  layoutSharpnessSpin_->setValue(preset.sharpnessWindow);
  layoutRoundingSpin_->setValue(preset.frameRoundingPx);
  layoutShadowSpin_->setValue(preset.frameShadowPx);
  layoutBorderSpin_->setValue(preset.frameBorderPx);
  layoutMetadataCheck_->setChecked(preset.showMetadataBar);
  layoutTimestampsCheck_->setChecked(preset.showTimestamps);
  layoutDarkModeCheck_->setChecked(preset.darkMode);
  watermarkTextEdit_->setText(preset.watermarkText);
  watermarkOpacitySpin_->setValue(preset.watermarkTextOpacity);
  watermarkImagePathEdit_->setText(preset.watermarkImagePath);
  watermarkImageOpacitySpin_->setValue(preset.watermarkImageOpacity);
  exportFormatCombo_->setCurrentText(preset.exportFormat);
  exportScaleSpin_->setValue(preset.exportScale);
  refreshLayoutTilesTable();
  suppressLayoutEditorUpdates_ = false;
}

void MainWindow::syncCurrentPresetFromEditors()
{
  if (suppressLayoutEditorUpdates_ || currentLayoutPresetIndex_ < 0 || currentLayoutPresetIndex_ >= layoutPresets_.size()) {
    return;
  }

  LayoutPresetState& preset = layoutPresets_[currentLayoutPresetIndex_];
  preset.name = layoutNameEdit_->text().trimmed().isEmpty() ? QStringLiteral("Unnamed Layout") : layoutNameEdit_->text().trimmed();
  preset.rows = layoutRowsSpin_->value();
  preset.columns = layoutColumnsSpin_->value();
  preset.gutterPx = layoutGutterSpin_->value();
  preset.outerMarginPx = layoutMarginSpin_->value();
  preset.sharpnessWindow = layoutSharpnessSpin_->value();
  preset.frameRoundingPx = layoutRoundingSpin_->value();
  preset.frameShadowPx = layoutShadowSpin_->value();
  preset.frameBorderPx = layoutBorderSpin_->value();
  preset.showMetadataBar = layoutMetadataCheck_->isChecked();
  preset.showTimestamps = layoutTimestampsCheck_->isChecked();
  preset.darkMode = layoutDarkModeCheck_->isChecked();
  preset.watermarkText = watermarkTextEdit_->text();
  preset.watermarkTextOpacity = watermarkOpacitySpin_->value();
  preset.watermarkImagePath = watermarkImagePathEdit_->text().trimmed();
  preset.watermarkImageOpacity = watermarkImageOpacitySpin_->value();
  preset.exportFormat = exportFormatCombo_->currentText();
  preset.exportScale = exportScaleSpin_->value();

  QVector<SheetTileLayoutState> tiles;
  tiles.reserve(layoutTilesTable_->rowCount());
  for (int row = 0; row < layoutTilesTable_->rowCount(); ++row) {
    SheetTileLayoutState tile;
    tile.id = layoutTilesTable_->item(row, 0)->text();
    tile.order = layoutTilesTable_->item(row, 1)->text().toInt();
    tile.row = layoutTilesTable_->item(row, 2)->text().toInt();
    tile.column = layoutTilesTable_->item(row, 3)->text().toInt();
    tile.rowSpan = std::max(1, layoutTilesTable_->item(row, 4)->text().toInt());
    tile.columnSpan = std::max(1, layoutTilesTable_->item(row, 5)->text().toInt());
    tiles.append(tile);
  }

  if (tiles.isEmpty() || static_cast<int>(tiles.size()) != preset.rows * preset.columns) {
    tiles = defaultTilesForGrid(preset.rows, preset.columns);
    refreshLayoutTilesTable();
  }
  preset.tiles = tiles;

  refreshLayoutPresetList();
  refreshQueuePanel();
  refreshLayoutPreview();
}

void MainWindow::refreshLayoutPresetList()
{
  const int currentIndex = currentLayoutPresetIndex_;
  layoutPresetList_->clear();
  queueLayoutCombo_->clear();
  for (const LayoutPresetState& preset : layoutPresets_) {
    layoutPresetList_->addItem(safeLayoutName(preset));
    queueLayoutCombo_->addItem(safeLayoutName(preset), preset.id);
  }

  if (currentIndex >= 0 && currentIndex < layoutPresetList_->count()) {
    layoutPresetList_->setCurrentRow(currentIndex);
    queueLayoutCombo_->setCurrentIndex(currentIndex);
  }
}

void MainWindow::refreshLayoutTilesTable()
{
  const LayoutPresetState* preset = currentLayoutPreset();
  if (!preset) {
    return;
  }

  const QSignalBlocker blocker(layoutTilesTable_);
  layoutTilesTable_->setRowCount(preset->tiles.size());
  for (int row = 0; row < preset->tiles.size(); ++row) {
    const SheetTileLayoutState& tile = preset->tiles[row];
    const std::array<QString, 6> values{
      tile.id,
      QString::number(tile.order),
      QString::number(tile.row),
      QString::number(tile.column),
      QString::number(tile.rowSpan),
      QString::number(tile.columnSpan),
    };
    for (int column = 0; column < static_cast<int>(values.size()); ++column) {
      auto* item = layoutTilesTable_->item(row, column);
      if (!item) {
        item = new QTableWidgetItem;
        layoutTilesTable_->setItem(row, column, item);
      }
      item->setText(values[column]);
    }
  }
}

void MainWindow::updateLayoutControlsEnabled()
{
  const bool hasPreset = currentLayoutPreset() != nullptr;
  applyLayoutCurrentButton_->setEnabled(hasPreset && activeSource());
  applyLayoutSelectedButton_->setEnabled(hasPreset && sourceTree_->selectedItems().size() > 1);
}

void MainWindow::refreshSourceBin()
{
  const QString filter = sourceFilterEdit_->text().trimmed();
  sourceTree_->setSortingEnabled(false);
  sourceTree_->clear();
  for (int index = 0; index < sources_.size(); ++index) {
    const ReviewSourceState& source = sources_[index];
    const QString layoutName = [&]() -> QString {
      const auto it = std::find_if(layoutPresets_.begin(), layoutPresets_.end(), [&](const LayoutPresetState& preset) {
        return preset.id == source.appliedLayoutPresetId;
      });
      return it == layoutPresets_.end() ? QStringLiteral("Unknown") : safeLayoutName(*it);
    }();

    auto* item = new QTreeWidgetItem(sourceTree_);
    item->setText(0, source.displayName);
    item->setText(1, statusForSource(source));
    item->setText(2, layoutName);
    item->setData(0, Qt::UserRole, source.path);
    if (!filter.isEmpty()) {
      const bool matches = source.displayName.contains(filter, Qt::CaseInsensitive)
        || source.path.contains(filter, Qt::CaseInsensitive)
        || layoutName.contains(filter, Qt::CaseInsensitive);
      item->setHidden(!matches);
    }
    if (index == activeSourceIndex_) {
      sourceTree_->setCurrentItem(item);
    }
  }
  sourceTree_->setSortingEnabled(true);
  sourceBinStatusLabel_->setText(QStringLiteral("%1 source(s)").arg(sources_.size()));
}

void MainWindow::refreshSourceBinFilter()
{
  refreshSourceBin();
}

void MainWindow::updateSourceTreeItem(int)
{
  refreshSourceBin();
}

void MainWindow::refreshQueuePanel()
{
  const bool batchMode = batchModeCheck_->isChecked();
  queuePanel_->setVisible(batchMode);
  if (!batchMode) {
    queueListWidget_->clear();
    queueStatusLabel_->setText("Batch mode is off.");
    updateLayoutControlsEnabled();
    return;
  }

  queueListWidget_->clear();
  const QList<QTreeWidgetItem*> items = sourceTree_->selectedItems();
  for (QTreeWidgetItem* item : items) {
    const int index = findSourceIndex(item->data(0, Qt::UserRole).toString());
    if (index < 0) {
      continue;
    }

    const ReviewSourceState& source = sources_[index];
    queueListWidget_->addItem(QStringLiteral("%1  |  %2").arg(source.displayName, statusForSource(source)));
  }

  queueStatusLabel_->setText(items.isEmpty()
    ? QStringLiteral("Select multiple sources to queue them for export.")
    : QStringLiteral("%1 queued source(s)").arg(items.size()));
  queueExportDirectoryEdit_->setText(batchExportDirectory_);
  queueExportButton_->setEnabled(!items.isEmpty());
  updateLayoutControlsEnabled();
}

void MainWindow::updateQuickSheetControls()
{
  const ReviewSourceState* source = activeSource();
  if (!source || source->selectedTileId.isEmpty()) {
    selectedTileLabel_->setText("No tile selected");
    selectedTileDetailsLabel_->setText("Click a tile in the sheet to assign or fine-tune its frame.");
    assignFrameButton_->setEnabled(false);
    pinTileButton_->setEnabled(false);
    sharpestButton_->setEnabled(false);
    return;
  }

  for (const ReviewTileState& tile : source->tiles) {
    if (tile.layout.id != source->selectedTileId) {
      continue;
    }

    selectedTileLabel_->setText(QStringLiteral("Selected tile: %1").arg(tile.layout.id));
    selectedTileDetailsLabel_->setText(QStringLiteral("Frame %1  |  %2  |  %3")
      .arg(tile.frameIndex + 1)
      .arg(formatTime(tile.timeMs))
      .arg(tile.pinned ? "Pinned" : "Auto"));
    assignFrameButton_->setEnabled(true);
    pinTileButton_->setEnabled(true);
    sharpestButton_->setEnabled(true);
    pinTileButton_->setText(tile.pinned ? "Unpin Tile" : "Pin Tile");
    return;
  }
}

void MainWindow::updateWorkspaceLabels()
{
  const ReviewSourceState* source = activeSource();
  titleLabel_->setText(source ? source->displayName : QStringLiteral("No source selected"));
  infoLabel_->setText(source && source->info.valid
    ? QStringLiteral("%1 ms  |  %2x%3  |  %4 fps  |  %5 frames")
        .arg(source->info.durationMs)
        .arg(source->info.width)
        .arg(source->info.height)
        .arg(source->info.fps, 0, 'f', 2)
        .arg(source->info.frameCount)
    : QStringLiteral("Add videos to start reviewing sources."));
}

void MainWindow::applyDarkPalette()
{
  qApp->setStyle("Fusion");
  setStyleSheet(R"(
    QMainWindow, QWidget {
      background: #10161d;
      color: #f4f7fb;
      font-family: "IBM Plex Sans", "Segoe UI", sans-serif;
      font-size: 13px;
    }
    QTabWidget::pane {
      border: none;
      background: #10161d;
    }
    QTabBar::tab {
      background: #16202a;
      border: 1px solid #273647;
      border-bottom: none;
      border-top-left-radius: 8px;
      border-top-right-radius: 8px;
      padding: 8px 18px;
      margin-right: 6px;
      color: #b7c5d3;
    }
    QTabBar::tab:selected {
      background: #203142;
      color: #f4f7fb;
    }
    QGroupBox {
      border: 1px solid #273647;
      border-radius: 10px;
      margin-top: 8px;
      padding: 10px 10px 10px 10px;
      background: #141d26;
      font-weight: 600;
    }
    QGroupBox::title {
      subcontrol-origin: margin;
      left: 10px;
      padding: 0 4px;
    }
    QPushButton, QSpinBox, QDoubleSpinBox, QComboBox, QLineEdit {
      min-height: 28px;
    }
    QPushButton {
      border: 1px solid #30465c;
      border-radius: 8px;
      background: #203142;
      padding: 4px 10px;
    }
    QPushButton:hover {
      background: #294055;
    }
    QPushButton:disabled {
      color: #738190;
      border-color: #233241;
      background: #18222c;
    }
    QTreeWidget, QListWidget, QTableWidget, QTextEdit, QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
      border: 1px solid #273647;
      border-radius: 8px;
      background: #0f151c;
      selection-background-color: #254463;
      selection-color: #f4f7fb;
    }
    QHeaderView::section {
      background: #16202a;
      color: #cbd8e4;
      border: none;
      border-right: 1px solid #273647;
      padding: 6px;
    }
    QScrollArea {
      border: none;
      background: transparent;
    }
    QLabel#SectionTitle {
      font-family: "Space Grotesk", "IBM Plex Sans", sans-serif;
      font-size: 22px;
      font-weight: 700;
    }
    QLabel#MetaLabel {
      color: #aab8c4;
    }
    QLabel#InspectorValue {
      color: #d8e3ee;
    }
  )");
}

void MainWindow::createUi()
{
  auto* central = new QWidget;
  auto* outerLayout = new QVBoxLayout(central);
  outerLayout->setContentsMargins(12, 12, 12, 12);
  outerLayout->setSpacing(10);

  workspaceTabs_ = new QTabWidget(central);
  outerLayout->addWidget(workspaceTabs_, 1);
  setCentralWidget(central);

  auto* reviewTab = new QWidget(workspaceTabs_);
  auto* reviewOuter = new QVBoxLayout(reviewTab);
  reviewOuter->setContentsMargins(0, 0, 0, 0);
  reviewOuter->setSpacing(10);

  reviewRootSplitter_ = new QSplitter(Qt::Horizontal, reviewTab);
  reviewRootSplitter_->setChildrenCollapsible(false);
  reviewOuter->addWidget(reviewRootSplitter_, 1);
  workspaceTabs_->addTab(reviewTab, "Review");

  sourceBinPanel_ = new QWidget(reviewRootSplitter_);
  auto* sourceBinLayout = new QVBoxLayout(sourceBinPanel_);
  sourceBinLayout->setContentsMargins(0, 0, 0, 0);
  sourceBinLayout->setSpacing(8);

  auto* sourceTitle = new QLabel("Source Bin");
  sourceTitle->setObjectName("SectionTitle");
  sourceBinLayout->addWidget(sourceTitle);

  sourceFilterEdit_ = new QLineEdit(sourceBinPanel_);
  sourceFilterEdit_->setPlaceholderText("Filter sources...");
  sourceBinLayout->addWidget(sourceFilterEdit_);

  auto* sourceButtons = new QHBoxLayout;
  auto* addVideosButton = new QPushButton("Add Videos");
  auto* removeSourceButton = new QPushButton("Remove");
  auto* revealSourceButton = new QPushButton("Reveal");
  batchModeCheck_ = new QCheckBox("Batch Mode");
  sourceButtons->addWidget(addVideosButton);
  sourceButtons->addWidget(removeSourceButton);
  sourceButtons->addWidget(revealSourceButton);
  sourceButtons->addStretch(1);
  sourceButtons->addWidget(batchModeCheck_);
  sourceBinLayout->addLayout(sourceButtons);

  sourceTree_ = new QTreeWidget(sourceBinPanel_);
  sourceTree_->setColumnCount(3);
  sourceTree_->setHeaderLabels({ "Source", "Status", "Layout" });
  sourceTree_->setRootIsDecorated(false);
  sourceTree_->setSelectionMode(QAbstractItemView::SingleSelection);
  sourceTree_->setSortingEnabled(true);
  sourceTree_->header()->setStretchLastSection(false);
  sourceTree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  sourceTree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  sourceTree_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  sourceBinLayout->addWidget(sourceTree_, 1);

  sourceBinStatusLabel_ = new QLabel("0 source(s)");
  sourceBinStatusLabel_->setObjectName("MetaLabel");
  sourceBinLayout->addWidget(sourceBinStatusLabel_);

  reviewContentSplitter_ = new QSplitter(Qt::Vertical, reviewRootSplitter_);
  reviewContentSplitter_->setChildrenCollapsible(false);

  auto* transportPane = new QWidget(reviewContentSplitter_);
  auto* transportLayout = new QVBoxLayout(transportPane);
  transportLayout->setContentsMargins(0, 0, 0, 0);
  transportLayout->setSpacing(10);

  auto* headerRow = new QHBoxLayout;
  titleLabel_ = new QLabel("No source selected");
  titleLabel_->setObjectName("SectionTitle");
  titleLabel_->setWordWrap(false);
  titleLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  infoLabel_ = new QLabel("Add videos to start the review workspace.");
  infoLabel_->setObjectName("MetaLabel");
  infoLabel_->setWordWrap(false);
  infoLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  infoLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  headerRow->addWidget(titleLabel_, 1);
  headerRow->addWidget(infoLabel_, 1);
  transportLayout->addLayout(headerRow);

  auto* playerHost = new QWidget(transportPane);
  playerHost->setMinimumHeight(360);
  playerHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto* playerStack = new QStackedLayout(playerHost);
  playerStack->setContentsMargins(0, 0, 0, 0);
  playerStack->setStackingMode(QStackedLayout::StackAll);

  mpvWidget_ = new MpvWidget(playerHost);
  cropOverlay_ = new CropOverlayWidget(playerHost);
  playerStack->addWidget(mpvWidget_);
  playerStack->addWidget(cropOverlay_);
  transportLayout->addWidget(playerHost, 1);

  auto* controlsRow = new QHBoxLayout;
  auto* playButton = new QPushButton(style()->standardIcon(QStyle::SP_MediaPlay), "Play");
  auto* pauseButton = new QPushButton(style()->standardIcon(QStyle::SP_MediaPause), "Pause");
  auto* stopButton = new QPushButton(style()->standardIcon(QStyle::SP_MediaStop), "Stop");
  auto* frameBackButton = new QPushButton("- Frame");
  auto* frameForwardButton = new QPushButton("+ Frame");
  auto* muteButton = new QPushButton("Muted");
  muteButton->setCheckable(true);
  muteButton->setChecked(true);
  auto* volumeSlider = new QSlider(Qt::Horizontal);
  volumeSlider->setRange(0, 100);
  volumeSlider->setValue(0);
  volumeSlider->setFixedWidth(120);
  controlsRow->addWidget(playButton);
  controlsRow->addWidget(pauseButton);
  controlsRow->addWidget(stopButton);
  controlsRow->addSpacing(12);
  controlsRow->addWidget(frameBackButton);
  controlsRow->addWidget(frameForwardButton);
  controlsRow->addStretch(1);
  controlsRow->addWidget(muteButton);
  controlsRow->addWidget(volumeSlider);
  transportLayout->addLayout(controlsRow);

  timelineWidget_ = new TimelineWidget(transportPane);
  transportLayout->addWidget(timelineWidget_);

  auto* metaRow = new QHBoxLayout;
  timeLabel_ = new QLabel("0:00.000 / 0:00.000");
  frameLabel_ = new QLabel("Frame 1");
  rangeLabel_ = new QLabel("Range 0:00.000 - 0:00.000");
  samplingStartLabel_ = new QLabel("Sampling 0:00.000");
  metaRow->addWidget(timeLabel_);
  metaRow->addStretch(1);
  metaRow->addWidget(rangeLabel_);
  metaRow->addStretch(1);
  metaRow->addWidget(samplingStartLabel_);
  metaRow->addStretch(1);
  metaRow->addWidget(frameLabel_);
  transportLayout->addLayout(metaRow);

  auto* cropRow = new QHBoxLayout;
  selectCropButton_ = new QPushButton("Select Crop");
  applyCropButton_ = new QPushButton("Apply");
  clearCropButton_ = new QPushButton("Clear");
  cropStatusLabel_ = new QLabel("No crop");
  cropStatusLabel_->setObjectName("MetaLabel");
  selectCropButton_->setEnabled(false);
  applyCropButton_->setEnabled(false);
  clearCropButton_->setEnabled(false);
  cropRow->addWidget(selectCropButton_);
  cropRow->addWidget(applyCropButton_);
  cropRow->addWidget(clearCropButton_);
  cropRow->addSpacing(12);
  cropRow->addWidget(cropStatusLabel_, 1);
  transportLayout->addLayout(cropRow);

  auto* jumpRow = new QHBoxLayout;
  customJumpSecondsSpin_ = new QDoubleSpinBox(transportPane);
  customJumpSecondsSpin_->setRange(0.04, 600.0);
  customJumpSecondsSpin_->setDecimals(3);
  customJumpSecondsSpin_->setSingleStep(0.5);
  customJumpSecondsSpin_->setValue(10.0);

  frameStepSpin_ = new QSpinBox(transportPane);
  frameStepSpin_->setRange(1, 100);
  frameStepSpin_->setValue(1);

  jumpRow->addWidget(new QLabel("Custom jump (s)"));
  jumpRow->addWidget(customJumpSecondsSpin_);
  jumpRow->addSpacing(12);
  jumpRow->addWidget(new QLabel("Frame step"));
  jumpRow->addWidget(frameStepSpin_);
  jumpRow->addStretch(1);
  transportLayout->addLayout(jumpRow);

  auto* quickButtonsRow = new QHBoxLayout;
  const auto addJumpButton = [&](const QString& label, const std::function<void()>& handler) {
    auto* button = new QPushButton(label);
    connect(button, &QPushButton::clicked, this, handler);
    quickButtonsRow->addWidget(button);
  };
  addJumpButton("- custom", [this] { mpvWidget_->seekRelativeMs(-customJumpMs()); });
  addJumpButton("+ custom", [this] { mpvWidget_->seekRelativeMs(customJumpMs()); });
  addJumpButton("- 5s", [this] { mpvWidget_->seekRelativeMs(-5000); });
  addJumpButton("+ 5s", [this] { mpvWidget_->seekRelativeMs(5000); });
  addJumpButton("- 1s", [this] { mpvWidget_->seekRelativeMs(-1000); });
  addJumpButton("+ 1s", [this] { mpvWidget_->seekRelativeMs(1000); });
  quickButtonsRow->addStretch(1);
  transportLayout->addLayout(quickButtonsRow);

  auto* sheetPane = new QWidget(reviewContentSplitter_);
  auto* sheetLayout = new QVBoxLayout(sheetPane);
  sheetLayout->setContentsMargins(0, 0, 0, 0);
  sheetLayout->setSpacing(10);

  auto* sheetBox = new QGroupBox("Interactive Sheet", sheetPane);
  auto* sheetBoxLayout = new QVBoxLayout(sheetBox);
  reviewSheetWidget_ = new InteractiveSheetWidget(sheetBox);
  sheetBoxLayout->addWidget(reviewSheetWidget_, 1);
  sheetLayout->addWidget(sheetBox, 1);

  auto* tileActionsBox = new QGroupBox("Tile Actions", sheetPane);
  auto* tileActionsLayout = new QHBoxLayout(tileActionsBox);
  auto* fineTuneBackButton = new QPushButton("-1 Frame");
  auto* fineTuneForwardButton = new QPushButton("+1 Frame");
  assignFrameButton_ = new QPushButton("Assign Current Frame");
  pinTileButton_ = new QPushButton("Pin Tile");
  sharpestButton_ = new QPushButton("Find Sharpest Neighbour");
  auto* tileInfoLayout = new QVBoxLayout;
  selectedTileLabel_ = new QLabel("No tile selected");
  selectedTileDetailsLabel_ = new QLabel("Click a tile in the sheet to assign or fine-tune its frame.");
  selectedTileDetailsLabel_->setObjectName("MetaLabel");
  tileInfoLayout->addWidget(selectedTileLabel_);
  tileInfoLayout->addWidget(selectedTileDetailsLabel_);
  tileActionsLayout->addLayout(tileInfoLayout, 1);
  tileActionsLayout->addWidget(fineTuneBackButton);
  tileActionsLayout->addWidget(fineTuneForwardButton);
  tileActionsLayout->addWidget(assignFrameButton_);
  tileActionsLayout->addWidget(pinTileButton_);
  tileActionsLayout->addWidget(sharpestButton_);
  sheetLayout->addWidget(tileActionsBox);

  queuePanel_ = new QWidget(reviewRootSplitter_);
  auto* queueLayout = new QVBoxLayout(queuePanel_);
  queueLayout->setContentsMargins(0, 0, 0, 0);
  queueLayout->setSpacing(8);
  auto* queueTitle = new QLabel("Batch Queue");
  queueTitle->setObjectName("SectionTitle");
  queueLayout->addWidget(queueTitle);
  auto* queuePresetRow = new QHBoxLayout;
  queuePresetRow->addWidget(new QLabel("Layout"));
  queueLayoutCombo_ = new QComboBox(queuePanel_);
  queuePresetRow->addWidget(queueLayoutCombo_, 1);
  queueLayout->addLayout(queuePresetRow);
  auto* queueExportRow = new QHBoxLayout;
  queueExportDirectoryEdit_ = new QLineEdit(queuePanel_);
  queueExportDirectoryEdit_->setPlaceholderText("Choose export directory...");
  auto* queueChooseDirButton = new QPushButton("Browse");
  queueExportRow->addWidget(queueExportDirectoryEdit_, 1);
  queueExportRow->addWidget(queueChooseDirButton);
  queueLayout->addLayout(queueExportRow);
  queueListWidget_ = new QListWidget(queuePanel_);
  queueLayout->addWidget(queueListWidget_, 1);
  queueStatusLabel_ = new QLabel("Batch mode is off.");
  queueStatusLabel_->setObjectName("MetaLabel");
  queueLayout->addWidget(queueStatusLabel_);
  queueExportButton_ = new QPushButton("Export Queue");
  queueLayout->addWidget(queueExportButton_);
  queuePanel_->hide();

  reviewRootSplitter_->addWidget(sourceBinPanel_);
  reviewRootSplitter_->addWidget(reviewContentSplitter_);
  reviewRootSplitter_->addWidget(queuePanel_);
  reviewRootSplitter_->setStretchFactor(0, 0);
  reviewRootSplitter_->setStretchFactor(1, 1);
  reviewRootSplitter_->setStretchFactor(2, 0);
  reviewRootSplitter_->setSizes({ 320, 1040, 0 });
  reviewContentSplitter_->setStretchFactor(0, 3);
  reviewContentSplitter_->setStretchFactor(1, 2);
  reviewContentSplitter_->setSizes({ 640, 400 });

  auto* layoutsTab = new QWidget(workspaceTabs_);
  auto* layoutsOuter = new QHBoxLayout(layoutsTab);
  layoutsOuter->setContentsMargins(0, 0, 0, 0);
  layoutsOuter->setSpacing(10);
  workspaceTabs_->addTab(layoutsTab, "Layouts");

  auto* presetsPane = new QWidget(layoutsTab);
  presetsPane->setMinimumWidth(260);
  auto* presetsLayout = new QVBoxLayout(presetsPane);
  presetsLayout->setContentsMargins(0, 0, 0, 0);
  presetsLayout->setSpacing(8);
  auto* presetsTitle = new QLabel("Layout Presets");
  presetsTitle->setObjectName("SectionTitle");
  presetsLayout->addWidget(presetsTitle);
  layoutPresetList_ = new QListWidget(presetsPane);
  presetsLayout->addWidget(layoutPresetList_, 1);
  auto* presetButtonsTop = new QHBoxLayout;
  auto* newLayoutButton = new QPushButton("New");
  auto* duplicateLayoutButton = new QPushButton("Duplicate");
  auto* renameLayoutButton = new QPushButton("Rename");
  auto* deleteLayoutButton = new QPushButton("Delete");
  presetButtonsTop->addWidget(newLayoutButton);
  presetButtonsTop->addWidget(duplicateLayoutButton);
  presetButtonsTop->addWidget(renameLayoutButton);
  presetButtonsTop->addWidget(deleteLayoutButton);
  presetsLayout->addLayout(presetButtonsTop);
  auto* presetButtonsBottom = new QHBoxLayout;
  auto* loadLayoutButton = new QPushButton("Load");
  auto* saveLayoutButton = new QPushButton("Save");
  auto* saveLayoutAsButton = new QPushButton("Save As");
  presetButtonsBottom->addWidget(loadLayoutButton);
  presetButtonsBottom->addWidget(saveLayoutButton);
  presetButtonsBottom->addWidget(saveLayoutAsButton);
  presetsLayout->addLayout(presetButtonsBottom);
  layoutsOuter->addWidget(presetsPane, 0);

  auto* layoutEditorPane = new QWidget(layoutsTab);
  auto* layoutEditorOuter = new QVBoxLayout(layoutEditorPane);
  layoutEditorOuter->setContentsMargins(0, 0, 0, 0);
  layoutEditorOuter->setSpacing(10);
  layoutsOuter->addWidget(layoutEditorPane, 1);

  auto* layoutPreviewBox = new QGroupBox("Layout Preview", layoutEditorPane);
  auto* layoutPreviewBoxLayout = new QVBoxLayout(layoutPreviewBox);
  layoutPreviewWidget_ = new InteractiveSheetWidget(layoutPreviewBox);
  layoutPreviewBoxLayout->addWidget(layoutPreviewWidget_, 1);
  layoutEditorOuter->addWidget(layoutPreviewBox, 1);

  auto* layoutScroll = new QScrollArea(layoutEditorPane);
  layoutScroll->setWidgetResizable(true);
  auto* layoutScrollContent = new QWidget(layoutScroll);
  auto* layoutScrollLayout = new QVBoxLayout(layoutScrollContent);
  layoutScrollLayout->setContentsMargins(0, 0, 0, 0);
  layoutScrollLayout->setSpacing(10);
  layoutScroll->setWidget(layoutScrollContent);
  layoutEditorOuter->addWidget(layoutScroll, 1);

  auto* identityBox = new QGroupBox("Preset Identity", layoutScrollContent);
  auto* identityLayout = new QFormLayout(identityBox);
  layoutNameEdit_ = new QLineEdit(identityBox);
  identityLayout->addRow("Name", layoutNameEdit_);
  layoutScrollLayout->addWidget(identityBox);

  auto* gridBox = new QGroupBox("Grid", layoutScrollContent);
  auto* gridLayout = new QFormLayout(gridBox);
  layoutRowsSpin_ = new QSpinBox(gridBox);
  layoutRowsSpin_->setRange(1, 12);
  layoutColumnsSpin_ = new QSpinBox(gridBox);
  layoutColumnsSpin_->setRange(1, 12);
  layoutGutterSpin_ = new QSpinBox(gridBox);
  layoutGutterSpin_->setRange(0, 96);
  layoutMarginSpin_ = new QSpinBox(gridBox);
  layoutMarginSpin_->setRange(0, 160);
  layoutSharpnessSpin_ = new QSpinBox(gridBox);
  layoutSharpnessSpin_->setRange(1, 60);
  gridLayout->addRow("Rows", layoutRowsSpin_);
  gridLayout->addRow("Columns", layoutColumnsSpin_);
  gridLayout->addRow("Gutter", layoutGutterSpin_);
  gridLayout->addRow("Outer Margin", layoutMarginSpin_);
  gridLayout->addRow("Sharpness Window", layoutSharpnessSpin_);
  layoutScrollLayout->addWidget(gridBox);

  auto* tilesBox = new QGroupBox("Tiles", layoutScrollContent);
  auto* tilesLayout = new QVBoxLayout(tilesBox);
  layoutTilesTable_ = new QTableWidget(tilesBox);
  layoutTilesTable_->setColumnCount(6);
  layoutTilesTable_->setHorizontalHeaderLabels({ "ID", "Order", "Row", "Col", "Row Span", "Col Span" });
  layoutTilesTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  layoutTilesTable_->verticalHeader()->setVisible(false);
  layoutTilesTable_->setMinimumHeight(220);
  tilesLayout->addWidget(layoutTilesTable_);
  layoutScrollLayout->addWidget(tilesBox);

  auto* styleBox = new QGroupBox("Style", layoutScrollContent);
  auto* styleLayout = new QFormLayout(styleBox);
  layoutRoundingSpin_ = new QSpinBox(styleBox);
  layoutRoundingSpin_->setRange(0, 96);
  layoutShadowSpin_ = new QSpinBox(styleBox);
  layoutShadowSpin_->setRange(0, 96);
  layoutBorderSpin_ = new QSpinBox(styleBox);
  layoutBorderSpin_->setRange(0, 24);
  layoutMetadataCheck_ = new QCheckBox("Show metadata bar", styleBox);
  layoutTimestampsCheck_ = new QCheckBox("Show timestamps", styleBox);
  layoutDarkModeCheck_ = new QCheckBox("Dark background", styleBox);
  styleLayout->addRow("Rounded Corners", layoutRoundingSpin_);
  styleLayout->addRow("Frame Shadow", layoutShadowSpin_);
  styleLayout->addRow("Frame Border", layoutBorderSpin_);
  styleLayout->addRow("", layoutMetadataCheck_);
  styleLayout->addRow("", layoutTimestampsCheck_);
  styleLayout->addRow("", layoutDarkModeCheck_);
  layoutScrollLayout->addWidget(styleBox);

  auto* watermarkBox = new QGroupBox("Watermark and Output", layoutScrollContent);
  auto* watermarkLayout = new QFormLayout(watermarkBox);
  watermarkTextEdit_ = new QLineEdit(watermarkBox);
  watermarkOpacitySpin_ = new QDoubleSpinBox(watermarkBox);
  watermarkOpacitySpin_->setRange(0.0, 1.0);
  watermarkOpacitySpin_->setSingleStep(0.05);
  watermarkOpacitySpin_->setDecimals(2);
  watermarkImagePathEdit_ = new QLineEdit(watermarkBox);
  auto* watermarkBrowseButton = new QPushButton("Browse");
  auto* watermarkImageRow = new QHBoxLayout;
  watermarkImageRow->addWidget(watermarkImagePathEdit_, 1);
  watermarkImageRow->addWidget(watermarkBrowseButton);
  auto* watermarkImageRowWidget = new QWidget(watermarkBox);
  watermarkImageRowWidget->setLayout(watermarkImageRow);
  watermarkImageOpacitySpin_ = new QDoubleSpinBox(watermarkBox);
  watermarkImageOpacitySpin_->setRange(0.0, 1.0);
  watermarkImageOpacitySpin_->setSingleStep(0.05);
  watermarkImageOpacitySpin_->setDecimals(2);
  exportFormatCombo_ = new QComboBox(watermarkBox);
  exportFormatCombo_->addItems({ "png", "jpeg" });
  exportScaleSpin_ = new QDoubleSpinBox(watermarkBox);
  exportScaleSpin_->setRange(0.25, 4.0);
  exportScaleSpin_->setSingleStep(0.25);
  exportScaleSpin_->setDecimals(2);
  watermarkLayout->addRow("Text", watermarkTextEdit_);
  watermarkLayout->addRow("Text Opacity", watermarkOpacitySpin_);
  watermarkLayout->addRow("Image", watermarkImageRowWidget);
  watermarkLayout->addRow("Image Opacity", watermarkImageOpacitySpin_);
  watermarkLayout->addRow("Export Format", exportFormatCombo_);
  watermarkLayout->addRow("Export Scale", exportScaleSpin_);
  layoutScrollLayout->addWidget(watermarkBox);

  auto* layoutActionsBox = new QGroupBox("Apply", layoutScrollContent);
  auto* layoutActionsLayout = new QHBoxLayout(layoutActionsBox);
  applyLayoutCurrentButton_ = new QPushButton("Apply to Current Source");
  applyLayoutSelectedButton_ = new QPushButton("Apply to Selected Sources");
  layoutActionsLayout->addWidget(applyLayoutCurrentButton_);
  layoutActionsLayout->addWidget(applyLayoutSelectedButton_);
  layoutScrollLayout->addWidget(layoutActionsBox);
  layoutScrollLayout->addStretch(1);

  devOverlay_ = new QDialog(this, Qt::Tool | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
  devOverlay_->setWindowTitle("Dev Overlay");
  devOverlay_->resize(420, 560);
  auto* devLayout = new QVBoxLayout(devOverlay_);
  devLayout->setContentsMargins(12, 12, 12, 12);
  devLayout->setSpacing(10);

  auto* sourceBox = new QGroupBox("Source", devOverlay_);
  auto* sourceLayout = new QFormLayout(sourceBox);
  sourceLayout->setContentsMargins(10, 12, 10, 10);
  backendLabel_ = new QLabel("Rust core bridge: unavailable");
  backendLabel_->setObjectName("InspectorValue");
  shellLabel_ = new QLabel("Qt 6 Widgets + libmpv");
  shellLabel_->setObjectName("InspectorValue");
  projectBridgeLabel_ = new QLabel("Rust CLI preview/probe bridge");
  projectBridgeLabel_->setWordWrap(true);
  projectBridgeLabel_->setObjectName("InspectorValue");
  sourceLayout->addRow("Backend", backendLabel_);
  sourceLayout->addRow("Shell", shellLabel_);
  sourceLayout->addRow("Bridge", projectBridgeLabel_);
  devLayout->addWidget(sourceBox);

  auto* logBox = new QGroupBox("Session Log", devOverlay_);
  auto* logLayout = new QVBoxLayout(logBox);
  logLayout->setContentsMargins(10, 12, 10, 10);
  statusText_ = new QTextEdit(logBox);
  statusText_->setReadOnly(true);
  statusText_->setMinimumHeight(180);
  statusText_->setPlainText(
    "Session log:\n"
    "- Native transport is authoritative.\n"
    "- Rust core handles preview rendering and export.\n");
  logLayout->addWidget(statusText_, 1);
  devLayout->addWidget(logBox, 1);
  devOverlay_->hide();

  connect(addVideosButton, &QPushButton::clicked, this, &MainWindow::addVideos);
  connect(removeSourceButton, &QPushButton::clicked, this, [this] {
    const QList<QTreeWidgetItem*> items = sourceTree_->selectedItems();
    if (items.isEmpty()) {
      return;
    }

    const QString currentPath = activeSource() ? activeSource()->path : QString();
    QVector<int> indices;
    for (QTreeWidgetItem* item : items) {
      const int index = findSourceIndex(item->data(0, Qt::UserRole).toString());
      if (index >= 0) {
        indices.append(index);
      }
    }
    std::sort(indices.begin(), indices.end(), std::greater<int>());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    for (int index : indices) {
      sources_.removeAt(index);
    }
    activeSourceIndex_ = sources_.isEmpty() ? -1 : std::clamp(activeSourceIndex_, 0, static_cast<int>(sources_.size()) - 1);
    refreshSourceBin();
    refreshQueuePanel();
    if (!sources_.isEmpty()) {
      const int nextIndex = findSourceIndex(currentPath);
      activateSourceByIndex(std::clamp(nextIndex >= 0 ? nextIndex : 0, 0, static_cast<int>(sources_.size()) - 1));
    } else {
      reviewSheetWidget_->clearPreview("Add a source to render the interactive sheet preview.");
      layoutPreviewWidget_->clearPreview("Load a source to preview this layout against real frames.");
      titleLabel_->setText("No source selected");
      infoLabel_->setText("Add videos to start the review workspace.");
    }
  });
  connect(revealSourceButton, &QPushButton::clicked, this, [this] {
    const ReviewSourceState* source = activeSource();
    if (!source) {
      return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(source->path).absolutePath()));
  });
  connect(sourceFilterEdit_, &QLineEdit::textChanged, this, [this] { refreshSourceBinFilter(); });
  connect(batchModeCheck_, &QCheckBox::toggled, this, [this](bool enabled) {
    sourceTree_->setSelectionMode(enabled ? QAbstractItemView::ExtendedSelection : QAbstractItemView::SingleSelection);
    refreshQueuePanel();
  });
  connect(sourceTree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem*, QTreeWidgetItem*) {
    activateCurrentSourceFromSelection();
  });
  connect(sourceTree_, &QTreeWidget::itemSelectionChanged, this, [this] {
    refreshQueuePanel();
    updateLayoutControlsEnabled();
  });

  connect(playButton, &QPushButton::clicked, this, &MainWindow::playSelectedRange);
  connect(pauseButton, &QPushButton::clicked, mpvWidget_, &MpvWidget::pause);
  connect(stopButton, &QPushButton::clicked, this, &MainWindow::stopSelectedRange);
  connect(muteButton, &QPushButton::toggled, this, [this, muteButton](bool muted) {
    mpvWidget_->setMuted(muted);
    muteButton->setText(muted ? "Muted" : "Mute");
  });
  connect(volumeSlider, &QSlider::valueChanged, this, [this, muteButton](int value) {
    mpvWidget_->setVolume(value);
    if (value > 0 && muteButton->isChecked()) {
      muteButton->setChecked(false);
    }
  });
  connect(frameBackButton, &QPushButton::clicked, this, [this] {
    mpvWidget_->stepFrames(-1, frameStepSpin_->value());
  });
  connect(frameForwardButton, &QPushButton::clicked, this, [this] {
    mpvWidget_->stepFrames(1, frameStepSpin_->value());
  });
  connect(selectCropButton_, &QPushButton::clicked, this, &MainWindow::beginCropSelection);
  connect(applyCropButton_, &QPushButton::clicked, this, &MainWindow::applyPendingCrop);
  connect(clearCropButton_, &QPushButton::clicked, this, &MainWindow::clearCrop);

  connect(timelineWidget_, &TimelineWidget::scrubPreviewRequested, this, [this](qint64 positionMs) {
    const qint64 durationMs = qMax<qint64>(timelineWidget_->durationMs(), mpvWidget_->durationMs());
    timeLabel_->setText(QStringLiteral("%1 / %2").arg(formatTime(positionMs), formatTime(durationMs)));
    frameLabel_->setText(QStringLiteral("Frame %1").arg(displayFrameNumber(positionMs)));
    mpvWidget_->seekPreviewMs(positionMs);
  });
  connect(timelineWidget_, &TimelineWidget::scrubFinished, this, [this](qint64 positionMs) {
    mpvWidget_->seekAbsoluteMs(positionMs);
  });
  connect(timelineWidget_, &TimelineWidget::rangePreviewChanged, this, [this](qint64 startMs, qint64 endMs) {
    updateSelectedRange(startMs, endMs, false);
  });
  connect(timelineWidget_, &TimelineWidget::rangeChangeFinished, this, [this](qint64 startMs, qint64 endMs) {
    updateSelectedRange(startMs, endMs, true);
    appendStatusMessage(QStringLiteral("Updated range to %1 - %2").arg(formatTime(startMs), formatTime(endMs)));
  });
  connect(timelineWidget_, &TimelineWidget::samplingStartPreviewChanged, this, [this](qint64 samplingStartMs) {
    updateSamplingStart(samplingStartMs, false);
  });
  connect(timelineWidget_, &TimelineWidget::samplingStartChangeFinished, this, [this](qint64 samplingStartMs) {
    updateSamplingStart(samplingStartMs, true);
    appendStatusMessage(QStringLiteral("Updated sampling start to %1").arg(formatTime(samplingStartMs)));
  });

  connect(mpvWidget_, &MpvWidget::positionChanged, this, &MainWindow::updateTransport);
  connect(mpvWidget_, &MpvWidget::playerError, this, [this](const QString& message) {
    appendStatusMessage(message);
    statusBar()->showMessage(message, 5000);
  });
  connect(cropOverlay_, &CropOverlayWidget::pendingCropChanged, this, [this](bool) {
    updateCropUi();
  });
  connect(cropOverlay_, &CropOverlayWidget::selectionModeChanged, this, [this](bool active) {
    selectCropButton_->setText(active ? "Selecting..." : "Select Crop");
  });
  connect(reviewSheetWidget_, &InteractiveSheetWidget::tileSelected, this, [this](const QString& tileId) {
    ReviewSourceState* source = activeSource();
    if (!source) {
      return;
    }

    source->selectedTileId = tileId;
    updateQuickSheetControls();
    for (const ReviewTileState& tile : source->tiles) {
      if (tile.layout.id == tileId) {
        mpvWidget_->seekAbsoluteMs(tile.timeMs);
        break;
      }
    }
  });
  connect(assignFrameButton_, &QPushButton::clicked, this, &MainWindow::assignCurrentFrameToSelectedTile);
  connect(pinTileButton_, &QPushButton::clicked, this, &MainWindow::toggleSelectedTilePinned);
  connect(sharpestButton_, &QPushButton::clicked, this, &MainWindow::findSharpestForSelectedTile);
  connect(fineTuneBackButton, &QPushButton::clicked, this, [this] { fineTuneSelectedTileFrames(-1); });
  connect(fineTuneForwardButton, &QPushButton::clicked, this, [this] { fineTuneSelectedTileFrames(1); });

  connect(queueChooseDirButton, &QPushButton::clicked, this, &MainWindow::chooseBatchExportDirectory);
  connect(queueExportButton_, &QPushButton::clicked, this, &MainWindow::exportBatchQueue);
  connect(queueLayoutCombo_, &QComboBox::currentIndexChanged, this, [this] { refreshQueuePanel(); });

  connect(layoutPresetList_, &QListWidget::currentRowChanged, this, &MainWindow::selectLayoutPreset);
  connect(newLayoutButton, &QPushButton::clicked, this, &MainWindow::createNewLayoutPreset);
  connect(duplicateLayoutButton, &QPushButton::clicked, this, &MainWindow::duplicateCurrentLayoutPreset);
  connect(renameLayoutButton, &QPushButton::clicked, this, &MainWindow::renameCurrentLayoutPreset);
  connect(deleteLayoutButton, &QPushButton::clicked, this, &MainWindow::deleteCurrentLayoutPreset);
  connect(loadLayoutButton, &QPushButton::clicked, this, &MainWindow::loadLayoutPresetFromFile);
  connect(saveLayoutButton, &QPushButton::clicked, this, &MainWindow::saveCurrentLayoutPreset);
  connect(saveLayoutAsButton, &QPushButton::clicked, this, &MainWindow::saveCurrentLayoutPresetAs);
  connect(applyLayoutCurrentButton_, &QPushButton::clicked, this, &MainWindow::applyLayoutToCurrentSource);
  connect(applyLayoutSelectedButton_, &QPushButton::clicked, this, &MainWindow::applyLayoutToSelectedSources);
  connect(watermarkBrowseButton, &QPushButton::clicked, this, &MainWindow::chooseWatermarkImage);

  connect(layoutNameEdit_, &QLineEdit::textChanged, this, [this] { syncCurrentPresetFromEditors(); });
  connect(layoutRowsSpin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) {
    if (suppressLayoutEditorUpdates_) {
      return;
    }
    layoutPresets_[currentLayoutPresetIndex_].tiles = defaultTilesForGrid(layoutRowsSpin_->value(), layoutColumnsSpin_->value());
    syncCurrentPresetFromEditors();
    refreshLayoutTilesTable();
  });
  connect(layoutColumnsSpin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) {
    if (suppressLayoutEditorUpdates_) {
      return;
    }
    layoutPresets_[currentLayoutPresetIndex_].tiles = defaultTilesForGrid(layoutRowsSpin_->value(), layoutColumnsSpin_->value());
    syncCurrentPresetFromEditors();
    refreshLayoutTilesTable();
  });
  connect(layoutGutterSpin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { syncCurrentPresetFromEditors(); });
  connect(layoutMarginSpin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { syncCurrentPresetFromEditors(); });
  connect(layoutSharpnessSpin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { syncCurrentPresetFromEditors(); });
  connect(layoutRoundingSpin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { syncCurrentPresetFromEditors(); });
  connect(layoutShadowSpin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { syncCurrentPresetFromEditors(); });
  connect(layoutBorderSpin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { syncCurrentPresetFromEditors(); });
  connect(layoutMetadataCheck_, &QCheckBox::toggled, this, [this](bool) { syncCurrentPresetFromEditors(); });
  connect(layoutTimestampsCheck_, &QCheckBox::toggled, this, [this](bool) { syncCurrentPresetFromEditors(); });
  connect(layoutDarkModeCheck_, &QCheckBox::toggled, this, [this](bool) { syncCurrentPresetFromEditors(); });
  connect(watermarkTextEdit_, &QLineEdit::textChanged, this, [this] { syncCurrentPresetFromEditors(); });
  connect(watermarkOpacitySpin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) { syncCurrentPresetFromEditors(); });
  connect(watermarkImagePathEdit_, &QLineEdit::textChanged, this, [this] { syncCurrentPresetFromEditors(); });
  connect(watermarkImageOpacitySpin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) { syncCurrentPresetFromEditors(); });
  connect(exportFormatCombo_, &QComboBox::currentTextChanged, this, [this](const QString&) { syncCurrentPresetFromEditors(); });
  connect(exportScaleSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) { syncCurrentPresetFromEditors(); });
  connect(layoutTilesTable_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem*) { syncCurrentPresetFromEditors(); });
}

void MainWindow::createMenuBar()
{
  auto* fileMenu = menuBar()->addMenu("&File");
  auto* addVideosAction = fileMenu->addAction("&Add Videos...");
  addVideosAction->setShortcut(QKeySequence::Open);
  connect(addVideosAction, &QAction::triggered, this, &MainWindow::addVideos);
  fileMenu->addSeparator();
  auto* loadLayoutAction = fileMenu->addAction("Load &Layout...");
  connect(loadLayoutAction, &QAction::triggered, this, &MainWindow::loadLayoutPresetFromFile);
  auto* saveLayoutAction = fileMenu->addAction("&Save Layout");
  connect(saveLayoutAction, &QAction::triggered, this, &MainWindow::saveCurrentLayoutPreset);
  auto* saveLayoutAsAction = fileMenu->addAction("Save Layout &As...");
  connect(saveLayoutAsAction, &QAction::triggered, this, &MainWindow::saveCurrentLayoutPresetAs);
  recentVideosMenu_ = fileMenu->addMenu("Recent &Videos");
  recentLayoutsMenu_ = fileMenu->addMenu("Recent &Layouts");
  fileMenu->addSeparator();
  auto* quitAction = fileMenu->addAction("&Quit");
  quitAction->setShortcut(QKeySequence::Quit);
  connect(quitAction, &QAction::triggered, this, &QWidget::close);

  auto* editMenu = menuBar()->addMenu("&Edit");
  auto* duplicateLayoutAction = editMenu->addAction("&Duplicate Layout");
  connect(duplicateLayoutAction, &QAction::triggered, this, &MainWindow::duplicateCurrentLayoutPreset);
  auto* renameLayoutAction = editMenu->addAction("&Rename Layout");
  connect(renameLayoutAction, &QAction::triggered, this, &MainWindow::renameCurrentLayoutPreset);
  auto* deleteLayoutAction = editMenu->addAction("&Delete Layout");
  connect(deleteLayoutAction, &QAction::triggered, this, &MainWindow::deleteCurrentLayoutPreset);

  auto* viewMenu = menuBar()->addMenu("&View");
  auto* toggleSourceBinAction = viewMenu->addAction("Toggle Source &Bin");
  connect(toggleSourceBinAction, &QAction::triggered, this, [this] {
    sourceBinPanel_->setVisible(!sourceBinPanel_->isVisible());
  });
  auto* toggleQueueAction = viewMenu->addAction("Toggle Batch &Queue");
  connect(toggleQueueAction, &QAction::triggered, this, [this] {
    queuePanel_->setVisible(!queuePanel_->isVisible());
  });
  auto* toggleDevOverlayAction = viewMenu->addAction("Toggle &Dev Overlay");
  toggleDevOverlayAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
  connect(toggleDevOverlayAction, &QAction::triggered, this, &MainWindow::toggleDevOverlay);

  auto* toolsMenu = menuBar()->addMenu("&Tools");
  auto* applyCurrentAction = toolsMenu->addAction("Apply Layout to &Current Source");
  connect(applyCurrentAction, &QAction::triggered, this, &MainWindow::applyLayoutToCurrentSource);
  auto* applySelectedAction = toolsMenu->addAction("Apply Layout to &Selected Sources");
  connect(applySelectedAction, &QAction::triggered, this, &MainWindow::applyLayoutToSelectedSources);
  auto* exportQueueAction = toolsMenu->addAction("&Export Queue");
  connect(exportQueueAction, &QAction::triggered, this, &MainWindow::exportBatchQueue);

  auto* playbackMenu = menuBar()->addMenu("&Playback");
  auto* playAction = playbackMenu->addAction("Play/&Pause");
  playAction->setShortcut(Qt::Key_Space);
  connect(playAction, &QAction::triggered, this, &MainWindow::togglePlayback);
  auto* stopAction = playbackMenu->addAction("&Stop");
  connect(stopAction, &QAction::triggered, this, &MainWindow::stopSelectedRange);
  playbackMenu->addSeparator();
  auto* stepBackAction = playbackMenu->addAction("Step &Backward");
  stepBackAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Left));
  connect(stepBackAction, &QAction::triggered, this, [this] { mpvWidget_->stepFrames(-1, frameStepSpin_->value()); });
  auto* stepForwardAction = playbackMenu->addAction("Step &Forward");
  stepForwardAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Right));
  connect(stepForwardAction, &QAction::triggered, this, [this] { mpvWidget_->stepFrames(1, frameStepSpin_->value()); });

  auto* helpMenu = menuBar()->addMenu("&Help");
  auto* shortcutsAction = helpMenu->addAction("&Shortcuts");
  connect(shortcutsAction, &QAction::triggered, this, [this] {
    QMessageBox::information(this, tr("Shortcuts"), tr("Space: Play/Pause\nCtrl+Left/Right: Frame step\nCtrl+Shift+D: Dev overlay"));
  });
  auto* aboutAction = helpMenu->addAction("&About");
  connect(aboutAction, &QAction::triggered, this, [this] {
    QMessageBox::information(this, tr("About"), tr("Video Preview Generator\nNative Qt + libmpv shell"));
  });

  rebuildRecentMenus();
}

void MainWindow::appendStatusMessage(const QString& message)
{
  if (statusText_) {
    statusText_->append(message);
  }
  statusBar()->showMessage(message, 4000);
}

void MainWindow::saveUiState() const
{
  QSettings settings;
  settings.setValue("workspace/currentTab", workspaceTabs_->currentIndex());
  settings.setValue("review/rootSplitter", reviewRootSplitter_->saveState());
  settings.setValue("review/contentSplitter", reviewContentSplitter_->saveState());
  settings.setValue("review/sourceBinVisible", sourceBinPanel_->isVisible());
  settings.setValue("review/queueVisible", queuePanel_->isVisible());
  settings.setValue("review/batchMode", batchModeCheck_->isChecked());
  settings.setValue("review/exportDirectory", batchExportDirectory_);
  settings.setValue("recent/videos", recentVideoPaths_);
  settings.setValue("recent/layouts", recentLayoutPaths_);
}

void MainWindow::restoreUiState()
{
  QSettings settings;
  workspaceTabs_->setCurrentIndex(settings.value("workspace/currentTab", 0).toInt());
  reviewRootSplitter_->restoreState(settings.value("review/rootSplitter").toByteArray());
  reviewContentSplitter_->restoreState(settings.value("review/contentSplitter").toByteArray());
  sourceBinPanel_->setVisible(settings.value("review/sourceBinVisible", true).toBool());
  queuePanel_->setVisible(settings.value("review/queueVisible", false).toBool());
  batchModeCheck_->setChecked(settings.value("review/batchMode", false).toBool());
  batchExportDirectory_ = settings.value("review/exportDirectory").toString();
  queueExportDirectoryEdit_->setText(batchExportDirectory_);
  recentVideoPaths_ = settings.value("recent/videos").toStringList();
  recentLayoutPaths_ = settings.value("recent/layouts").toStringList();
  rebuildRecentMenus();
}

QString MainWindow::formatTime(qint64 timeMs) const
{
  const qint64 clamped = qMax<qint64>(0, timeMs);
  const qint64 totalSeconds = clamped / 1000;
  const qint64 minutes = totalSeconds / 60;
  const qint64 seconds = totalSeconds % 60;
  const qint64 milliseconds = clamped % 1000;
  return QStringLiteral("%1:%2.%3")
    .arg(minutes)
    .arg(seconds, 2, 10, QLatin1Char('0'))
    .arg(milliseconds, 3, 10, QLatin1Char('0'));
}

qint64 MainWindow::customJumpMs() const
{
  return static_cast<qint64>(customJumpSecondsSpin_->value() * 1000.0);
}

int MainWindow::displayFrameNumber(qint64 positionMs) const
{
  if (projectInfo_.fps <= 0.0) {
    return 1;
  }

  const double frameIndex = std::floor(((static_cast<double>(positionMs) + 0.5) / 1000.0) * projectInfo_.fps);
  return static_cast<int>(frameIndex) + 1;
}

int MainWindow::findSourceIndex(const QString& path) const
{
  const QString normalized = normalizePath(path);
  for (int index = 0; index < sources_.size(); ++index) {
    if (sources_[index].path == normalized) {
      return index;
    }
  }
  return -1;
}

int MainWindow::activeSourceIndex() const
{
  return activeSourceIndex_;
}

ReviewSourceState* MainWindow::activeSource()
{
  return activeSourceIndex_ >= 0 && activeSourceIndex_ < sources_.size() ? &sources_[activeSourceIndex_] : nullptr;
}

const ReviewSourceState* MainWindow::activeSource() const
{
  return activeSourceIndex_ >= 0 && activeSourceIndex_ < sources_.size() ? &sources_[activeSourceIndex_] : nullptr;
}

LayoutPresetState* MainWindow::currentLayoutPreset()
{
  return currentLayoutPresetIndex_ >= 0 && currentLayoutPresetIndex_ < layoutPresets_.size()
    ? &layoutPresets_[currentLayoutPresetIndex_]
    : nullptr;
}

const LayoutPresetState* MainWindow::currentLayoutPreset() const
{
  return currentLayoutPresetIndex_ >= 0 && currentLayoutPresetIndex_ < layoutPresets_.size()
    ? &layoutPresets_[currentLayoutPresetIndex_]
    : nullptr;
}

LayoutPresetState* MainWindow::layoutPresetById(const QString& id)
{
  for (LayoutPresetState& preset : layoutPresets_) {
    if (preset.id == id) {
      return &preset;
    }
  }
  return nullptr;
}

const LayoutPresetState* MainWindow::layoutPresetById(const QString& id) const
{
  for (const LayoutPresetState& preset : layoutPresets_) {
    if (preset.id == id) {
      return &preset;
    }
  }
  return nullptr;
}

const LayoutPresetState* MainWindow::appliedLayoutForSource(const ReviewSourceState& source) const
{
  if (const LayoutPresetState* preset = layoutPresetById(source.appliedLayoutPresetId)) {
    return preset;
  }
  return currentLayoutPreset();
}

LayoutPresetState MainWindow::defaultLayoutPreset() const
{
  LayoutPresetState preset;
  preset.id = QStringLiteral("default-layout");
  preset.name = QStringLiteral("Default Grid");
  preset.tiles = defaultTilesForGrid(preset.rows, preset.columns);
  return preset;
}

QVector<SheetTileLayoutState> MainWindow::defaultTilesForGrid(int rows, int columns) const
{
  QVector<SheetTileLayoutState> tiles;
  tiles.reserve(rows * columns);
  int order = 0;
  for (int row = 0; row < rows; ++row) {
    for (int column = 0; column < columns; ++column) {
      tiles.append(SheetTileLayoutState{
        QStringLiteral("tile-%1").arg(order),
        order,
        row,
        column,
        1,
        1,
      });
      order += 1;
    }
  }
  return tiles;
}

void MainWindow::applyLayoutToSourceState(ReviewSourceState& source, const LayoutPresetState& preset, bool preservePinned)
{
  QHash<QString, ReviewTileState> preserved;
  if (preservePinned) {
    for (const ReviewTileState& tile : source.tiles) {
      if (tile.pinned) {
        preserved.insert(tile.layout.id, tile);
      }
    }
  }

  source.tiles.clear();
  source.appliedLayoutPresetId = preset.id;
  const QVector<SheetTileLayoutState> sortedTiles = preset.tiles;
  for (const SheetTileLayoutState& layoutTile : sortedTiles) {
    ReviewTileState tile;
    tile.layout = layoutTile;
    if (preserved.contains(layoutTile.id)) {
      tile = preserved.value(layoutTile.id);
      tile.layout = layoutTile;
    }
    source.tiles.append(tile);
  }

  if (source.selectedTileId.isEmpty() && !source.tiles.isEmpty()) {
    source.selectedTileId = source.tiles.first().layout.id;
  } else if (!source.selectedTileId.isEmpty()) {
    const bool stillExists = std::any_of(source.tiles.begin(), source.tiles.end(), [&](const ReviewTileState& tile) {
      return tile.layout.id == source.selectedTileId;
    });
    if (!stillExists && !source.tiles.isEmpty()) {
      source.selectedTileId = source.tiles.first().layout.id;
    }
  }

  if (source.info.valid) {
    if (source.rangeEndMs <= source.rangeStartMs) {
      source.rangeStartMs = 0;
      source.rangeEndMs = source.info.durationMs;
      source.samplingStartMs = source.rangeStartMs;
    }
    reassignAutoTiles(source);
  }
}

void MainWindow::reassignAutoTiles(ReviewSourceState& source)
{
  if (!source.info.valid || source.info.fps <= 0.0) {
    return;
  }

  QVector<int> autoIndices;
  for (int index = 0; index < source.tiles.size(); ++index) {
    if (!source.tiles[index].pinned) {
      autoIndices.append(index);
    }
  }

  if (autoIndices.isEmpty()) {
    return;
  }

  const qint64 rangeStart = std::max<qint64>(0, source.rangeStartMs);
  const qint64 rangeEnd = std::max<qint64>(rangeStart + 1, source.rangeEndMs);
  const int count = autoIndices.size();
  const qint64 safeStart = count == 1
    ? std::clamp<qint64>(source.samplingStartMs, rangeStart, rangeEnd)
    : std::clamp<qint64>(source.samplingStartMs, rangeStart, std::max<qint64>(rangeStart, rangeEnd - 1));
  const double span = static_cast<double>(rangeEnd - safeStart);

  for (int sampleIndex = 0; sampleIndex < count; ++sampleIndex) {
    const qint64 sampleMs = count == 1
      ? safeStart
      : safeStart + static_cast<qint64>(std::llround(
          (sampleIndex * span) / static_cast<double>(std::max(1, count - 1))));
    ReviewTileState& tile = source.tiles[autoIndices[sampleIndex]];
    tile.frameIndex = frameIndexAtTime(sampleMs, source.info.fps, source.info.frameCount);
    tile.timeMs = seekTimeForFrameIndex(tile.frameIndex, source.info.durationMs, source.info.fps);
    tile.fineTuneOffsetMs = 0;
  }
}

qint64 MainWindow::seekTimeForFrameIndex(qint64 frameIndex, qint64 durationMs, double fps) const
{
  if (fps <= 0.0) {
    return 0;
  }

  const qint64 safeFrameIndex = std::max<qint64>(0, frameIndex);
  if (safeFrameIndex == 0) {
    return 0;
  }

  const qint64 frameStartMs = static_cast<qint64>(std::llround((static_cast<double>(safeFrameIndex) / fps) * 1000.0));
  return std::min(frameStartMs, std::max<qint64>(0, durationMs - 1));
}

qint64 MainWindow::frameIndexAtTime(qint64 timeMs, double fps, qint64 frameCount) const
{
  if (fps <= 0.0) {
    return 0;
  }

  qint64 frameIndex = static_cast<qint64>(std::floor(((static_cast<double>(timeMs) + 0.5) / 1000.0) * fps));
  frameIndex = std::max<qint64>(0, frameIndex);
  if (frameCount > 0) {
    frameIndex = std::min<qint64>(frameIndex, frameCount - 1);
  }
  return frameIndex;
}

SheetLayoutPreviewState MainWindow::sheetLayoutPreviewState(const LayoutPresetState& preset, const ReviewSourceState* source) const
{
  SheetLayoutPreviewState state;
  state.rows = preset.rows;
  state.columns = preset.columns;
  state.gutterPx = preset.gutterPx;
  state.outerMarginPx = preset.outerMarginPx;
  state.showMetadataBar = preset.showMetadataBar;
  state.exportScale = preset.exportScale;
  state.sourceAspectRatio = source && source->info.valid
    ? effectivePreviewAspectRatio(source->info, source->crop)
    : 16.0 / 9.0;
  state.tiles = preset.tiles;
  return state;
}

QJsonObject MainWindow::buildProjectJson(const ReviewSourceState& source, const LayoutPresetState& preset) const
{
  const qint64 playheadMs = (activeSource() && activeSource()->path == source.path)
    ? mpvWidget_->currentTimeMs()
    : source.rangeStartMs;
  QJsonArray tiles;
  for (const ReviewTileState& tile : source.tiles) {
    QJsonObject tileObject;
    tileObject.insert("id", tile.layout.id);
    tileObject.insert("order", tile.layout.order);
    tileObject.insert("span", QJsonObject{
      { "row", tile.layout.row },
      { "column", tile.layout.column },
      { "rowSpan", tile.layout.rowSpan },
      { "columnSpan", tile.layout.columnSpan },
    });
    tileObject.insert("selection", QJsonObject{
      { "kind", "manual" },
      { "frameIndex", static_cast<double>(tile.frameIndex) },
      { "timeMs", static_cast<double>(tile.timeMs) },
    });
    tileObject.insert("pinned", tile.pinned);
    tileObject.insert("fineTuneOffsetMs", static_cast<double>(tile.fineTuneOffsetMs));
    tiles.append(tileObject);
  }

  QJsonObject video{
    { "path", source.path },
    { "durationMs", static_cast<double>(source.info.durationMs) },
    { "fps", source.info.fps },
    { "frameCount", static_cast<double>(source.info.frameCount) },
    { "width", source.info.width },
    { "height", source.info.height },
  };
  if (source.crop.has_value()) {
    video.insert("crop", QJsonObject{
      { "x", source.crop->x() },
      { "y", source.crop->y() },
      { "width", source.crop->width() },
      { "height", source.crop->height() },
    });
  }

  QJsonObject watermark{
    { "text", preset.watermarkText.isEmpty()
        ? QJsonValue(QJsonValue::Null)
        : QJsonObject{
            { "value", preset.watermarkText },
            { "opacity", preset.watermarkTextOpacity },
          } },
    { "image", preset.watermarkImagePath.isEmpty()
        ? QJsonValue(QJsonValue::Null)
        : QJsonObject{
            { "path", preset.watermarkImagePath },
            { "opacity", preset.watermarkImageOpacity },
          } },
  };

  return QJsonObject{
    { "version", 1 },
    { "analysisMode", "quick_preview" },
    { "video", video },
    { "playback", QJsonObject{
        { "playheadMs", static_cast<double>(playheadMs) },
        { "activeFrameIndex", static_cast<double>(frameIndexAtTime(playheadMs, source.info.fps, source.info.frameCount)) },
        { "customSkipMs", static_cast<double>(customJumpMs()) },
        { "frameStep", frameStepSpin_->value() },
      } },
    { "range", QJsonObject{
        { "startMs", static_cast<double>(source.rangeStartMs) },
        { "endMs", static_cast<double>(source.rangeEndMs) },
        { "sampleStartMs", static_cast<double>(source.samplingStartMs) },
      } },
    { "grid", QJsonObject{
        { "rows", preset.rows },
        { "columns", preset.columns },
        { "gutterPx", preset.gutterPx },
        { "outerMarginPx", preset.outerMarginPx },
        { "defaultSharpnessWindow", preset.sharpnessWindow },
      } },
    { "tiles", tiles },
    { "style", QJsonObject{
        { "frameRoundingPx", preset.frameRoundingPx },
        { "frameShadowPx", preset.frameShadowPx },
        { "frameBorderPx", preset.frameBorderPx },
        { "showMetadataBar", preset.showMetadataBar },
        { "showTimestamps", preset.showTimestamps },
        { "darkMode", preset.darkMode },
      } },
    { "watermark", watermark },
    { "export", QJsonObject{
        { "format", preset.exportFormat.toLower() },
        { "scale", preset.exportScale },
        { "outputPath", source.lastExportPath.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(source.lastExportPath) },
      } },
    { "batch", QJsonObject{
        { "enabled", batchModeCheck_->isChecked() },
        { "retainManualOverrides", true },
        { "inputs", QJsonArray::fromStringList({ source.path }) },
      } },
  };
}

void MainWindow::updateSourceFromProjectJson(ReviewSourceState& source, const QJsonObject& project)
{
  const QJsonArray tiles = project.value("tiles").toArray();
  for (const QJsonValue& value : tiles) {
    const QJsonObject tileObject = value.toObject();
    const QString id = tileObject.value("id").toString();
    for (ReviewTileState& tile : source.tiles) {
      if (tile.layout.id != id) {
        continue;
      }

      const QJsonObject selection = tileObject.value("selection").toObject();
      tile.frameIndex = static_cast<qint64>(selection.value("frameIndex").toDouble(tile.frameIndex));
      tile.timeMs = static_cast<qint64>(selection.value("timeMs").toDouble(tile.timeMs));
      tile.pinned = tileObject.value("pinned").toBool(tile.pinned);
      tile.fineTuneOffsetMs = static_cast<qint64>(tileObject.value("fineTuneOffsetMs").toDouble(tile.fineTuneOffsetMs));
      break;
    }
  }
}

QJsonObject MainWindow::buildLayoutPresetJson(const LayoutPresetState& preset) const
{
  QJsonArray tiles;
  for (const SheetTileLayoutState& tile : preset.tiles) {
    tiles.append(QJsonObject{
      { "id", tile.id },
      { "order", tile.order },
      { "span", QJsonObject{
          { "row", tile.row },
          { "column", tile.column },
          { "rowSpan", tile.rowSpan },
          { "columnSpan", tile.columnSpan },
        } },
    });
  }

  return QJsonObject{
    { "version", 1 },
    { "name", safeLayoutName(preset) },
    { "grid", QJsonObject{
        { "rows", preset.rows },
        { "columns", preset.columns },
        { "gutterPx", preset.gutterPx },
        { "outerMarginPx", preset.outerMarginPx },
        { "defaultSharpnessWindow", preset.sharpnessWindow },
      } },
    { "tiles", tiles },
    { "style", QJsonObject{
        { "frameRoundingPx", preset.frameRoundingPx },
        { "frameShadowPx", preset.frameShadowPx },
        { "frameBorderPx", preset.frameBorderPx },
        { "showMetadataBar", preset.showMetadataBar },
        { "showTimestamps", preset.showTimestamps },
        { "darkMode", preset.darkMode },
      } },
    { "watermark", QJsonObject{
        { "text", preset.watermarkText.isEmpty()
            ? QJsonValue(QJsonValue::Null)
            : QJsonObject{
                { "value", preset.watermarkText },
                { "opacity", preset.watermarkTextOpacity },
              } },
        { "image", preset.watermarkImagePath.isEmpty()
            ? QJsonValue(QJsonValue::Null)
            : QJsonObject{
                { "path", preset.watermarkImagePath },
                { "opacity", preset.watermarkImageOpacity },
              } },
      } },
    { "export", QJsonObject{
        { "format", preset.exportFormat.toLower() },
        { "scale", preset.exportScale },
        { "outputPath", QJsonValue(QJsonValue::Null) },
      } },
  };
}

LayoutPresetState MainWindow::parseLayoutPresetJson(const QJsonObject& object) const
{
  LayoutPresetState preset = defaultLayoutPreset();
  preset.name = object.value("name").toString(preset.name);
  preset.id = sanitizedStem(preset.name);

  const QJsonObject grid = object.value("grid").toObject();
  preset.rows = grid.value("rows").toInt(preset.rows);
  preset.columns = grid.value("columns").toInt(preset.columns);
  preset.gutterPx = grid.value("gutterPx").toInt(preset.gutterPx);
  preset.outerMarginPx = grid.value("outerMarginPx").toInt(preset.outerMarginPx);
  preset.sharpnessWindow = grid.value("defaultSharpnessWindow").toInt(preset.sharpnessWindow);

  const QJsonObject style = object.value("style").toObject();
  preset.frameRoundingPx = style.value("frameRoundingPx").toInt(preset.frameRoundingPx);
  preset.frameShadowPx = style.value("frameShadowPx").toInt(preset.frameShadowPx);
  preset.frameBorderPx = style.value("frameBorderPx").toInt(preset.frameBorderPx);
  preset.showMetadataBar = style.value("showMetadataBar").toBool(preset.showMetadataBar);
  preset.showTimestamps = style.value("showTimestamps").toBool(preset.showTimestamps);
  preset.darkMode = style.value("darkMode").toBool(preset.darkMode);

  const QJsonObject watermark = object.value("watermark").toObject();
  if (watermark.value("text").isObject()) {
    const QJsonObject text = watermark.value("text").toObject();
    preset.watermarkText = text.value("value").toString();
    preset.watermarkTextOpacity = text.value("opacity").toDouble(preset.watermarkTextOpacity);
  } else {
    preset.watermarkText.clear();
  }
  if (watermark.value("image").isObject()) {
    const QJsonObject image = watermark.value("image").toObject();
    preset.watermarkImagePath = image.value("path").toString();
    preset.watermarkImageOpacity = image.value("opacity").toDouble(preset.watermarkImageOpacity);
  } else {
    preset.watermarkImagePath.clear();
  }

  const QJsonObject exportObject = object.value("export").toObject();
  preset.exportFormat = exportObject.value("format").toString(preset.exportFormat);
  preset.exportScale = exportObject.value("scale").toDouble(preset.exportScale);

  const QJsonArray tiles = object.value("tiles").toArray();
  QVector<SheetTileLayoutState> parsedTiles;
  for (const QJsonValue& value : tiles) {
    const QJsonObject tileObject = value.toObject();
    const QJsonObject span = tileObject.value("span").toObject();
    parsedTiles.append(SheetTileLayoutState{
      tileObject.value("id").toString(QStringLiteral("tile-%1").arg(parsedTiles.size())),
      tileObject.value("order").toInt(parsedTiles.size()),
      span.value("row").toInt(),
      span.value("column").toInt(),
      std::max(1, span.value("rowSpan").toInt(1)),
      std::max(1, span.value("columnSpan").toInt(1)),
    });
  }
  if (!parsedTiles.isEmpty()) {
    preset.tiles = parsedTiles;
  } else {
    preset.tiles = defaultTilesForGrid(preset.rows, preset.columns);
  }

  return preset;
}

void MainWindow::updateRecentVideos(const QString& path)
{
  recentVideoPaths_.removeAll(path);
  recentVideoPaths_.prepend(path);
  while (recentVideoPaths_.size() > 10) {
    recentVideoPaths_.removeLast();
  }
  rebuildRecentMenus();
}

void MainWindow::updateRecentLayouts(const QString& path)
{
  recentLayoutPaths_.removeAll(path);
  recentLayoutPaths_.prepend(path);
  while (recentLayoutPaths_.size() > 10) {
    recentLayoutPaths_.removeLast();
  }
  rebuildRecentMenus();
}

void MainWindow::rebuildRecentMenus()
{
  if (!recentVideosMenu_ || !recentLayoutsMenu_) {
    return;
  }

  recentVideosMenu_->clear();
  if (recentVideoPaths_.isEmpty()) {
    auto* empty = recentVideosMenu_->addAction("No recent videos");
    empty->setEnabled(false);
  } else {
    for (const QString& path : recentVideoPaths_) {
      auto* action = recentVideosMenu_->addAction(QFileInfo(path).fileName());
      action->setToolTip(path);
      connect(action, &QAction::triggered, this, [this, path] { loadVideo(path); });
    }
  }

  recentLayoutsMenu_->clear();
  if (recentLayoutPaths_.isEmpty()) {
    auto* empty = recentLayoutsMenu_->addAction("No recent layouts");
    empty->setEnabled(false);
  } else {
    for (const QString& path : recentLayoutPaths_) {
      auto* action = recentLayoutsMenu_->addAction(QFileInfo(path).fileName());
      action->setToolTip(path);
      connect(action, &QAction::triggered, this, [this, path] {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
          return;
        }
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        if (!document.isObject()) {
          return;
        }
        LayoutPresetState preset = parseLayoutPresetJson(document.object());
        preset.filePath = path;
        layoutPresets_.append(preset);
        currentLayoutPresetIndex_ = layoutPresets_.size() - 1;
        refreshLayoutPresetList();
        syncLayoutEditorsFromCurrentPreset();
        refreshLayoutPreview();
      });
    }
  }
}
