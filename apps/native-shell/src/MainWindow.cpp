#include "MainWindow.hpp"

#include <QAction>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPixmap>
#include <QPushButton>
#include <QStackedLayout>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QSpinBox>
#include <QtConcurrent>
#include <cmath>
#include <functional>

#include "CropOverlayWidget.hpp"
#include "MpvWidget.hpp"
#include "TimelineWidget.hpp"

namespace {

QWidget* makeSectionHeader(const QString& eyebrow, const QString& title, const QString& description)
{
  auto* widget = new QWidget;
  auto* layout = new QVBoxLayout(widget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);

  auto* eyebrowLabel = new QLabel(eyebrow);
  eyebrowLabel->setObjectName("EyebrowLabel");

  auto* titleLabel = new QLabel(title);
  titleLabel->setObjectName("SectionTitle");

  auto* descriptionLabel = new QLabel(description);
  descriptionLabel->setObjectName("SectionDescription");
  descriptionLabel->setWordWrap(true);

  layout->addWidget(eyebrowLabel);
  layout->addWidget(titleLabel);
  layout->addWidget(descriptionLabel);
  return widget;
}

struct BackgroundLoadResult {
  int requestId = 0;
  QString videoPath;
  ProjectInfo info;
  QString inspectError;
};

} // namespace

MainWindow::MainWindow(QWidget* parent)
  : QMainWindow(parent)
{
  setWindowTitle("Video Preview Generator");
  resize(1600, 960);
  applyDarkPalette();
  createUi();
  createMenuBar();
  statusBar()->showMessage("Native shell ready");
}

void MainWindow::openVideo()
{
  const QString path = QFileDialog::getOpenFileName(
    this,
    tr("Open Video"),
    QString(),
    tr("Video Files (*.mp4 *.mkv *.mov *.avi *.webm *.m4v);;All Files (*)"));
  if (path.isEmpty()) {
    return;
  }

  loadRequestId_ += 1;
  previewRequestId_ = 0;
  const QFileInfo fileInfo(path);
  projectInfo_ = {};
  projectInfo_.videoPath = path;
  projectInfo_.displayName = fileInfo.fileName();
  projectInfo_.valid = true;
  appliedCrop_.reset();
  titleLabel_->setText(projectInfo_.displayName);
  infoLabel_->setText("Loading metadata in the background...");
  updateTransport(0, 0);
  sheetPreviewLabel_->setPixmap(QPixmap());
  sheetPreviewLabel_->setText("Rendering starter sheet preview in the background...");
  cropOverlay_->setSourceVideoSize(QSize());
  cropOverlay_->setAppliedCrop(std::nullopt);
  cropOverlay_->clearPendingCrop();
  updateCropUi();
  appendStatusMessage(QStringLiteral("Loading %1...").arg(projectInfo_.displayName));
  mpvWidget_->loadFile(path);
  beginBackgroundLoad(path);
}

void MainWindow::beginBackgroundLoad(const QString& path)
{
  const int requestId = loadRequestId_;
  auto* watcher = new QFutureWatcher<BackgroundLoadResult>(this);

  connect(watcher, &QFutureWatcher<BackgroundLoadResult>::finished, this, [this, watcher] {
    const BackgroundLoadResult result = watcher->result();
    watcher->deleteLater();

    if (result.requestId != loadRequestId_) {
      return;
    }

    if (result.info.valid) {
      updateMetadata(result.info);
      appendStatusMessage(QStringLiteral("Loaded %1 via Rust CLI metadata bridge.").arg(result.info.displayName));
      beginPreviewRender();
    } else if (!result.inspectError.isEmpty()) {
      infoLabel_->setText("Metadata probe failed.");
      appendStatusMessage(result.inspectError);
      sheetPreviewLabel_->setPixmap(QPixmap());
      sheetPreviewLabel_->setText("Failed to inspect video metadata.");
    }
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
  if (!projectInfo_.valid || projectInfo_.videoPath.isEmpty()) {
    return;
  }

  previewRequestId_ += 1;
  const int requestId = previewRequestId_;
  const QString videoPath = projectInfo_.videoPath;
  const std::optional<QRectF> crop = appliedCrop_;

  sheetPreviewLabel_->setPixmap(QPixmap());
  sheetPreviewLabel_->setText(crop.has_value()
    ? "Rendering cropped sheet preview in the background..."
    : "Rendering starter sheet preview in the background...");

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

    sheetPreviewLabel_->setPixmap(QPixmap());
    sheetPreviewLabel_->setText(previewError.isEmpty()
      ? "Failed to render sheet preview."
      : previewError);
    appendStatusMessage(sheetPreviewLabel_->text());
  });

  watcher->setFuture(QtConcurrent::run([videoPath, crop]() {
    QString errorMessage;
    RustBridge bridge;
    const QString previewPath = bridge.renderStarterPreview(videoPath, &errorMessage, 1100, crop);
    return qMakePair(previewPath, errorMessage);
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
  const std::optional<QRectF> pendingCrop = cropOverlay_->pendingCrop();
  if (!pendingCrop.has_value()) {
    return;
  }

  appliedCrop_ = pendingCrop;
  cropOverlay_->setAppliedCrop(appliedCrop_);
  updateCropUi();
  refreshSheetPreview();
  appendStatusMessage(QStringLiteral(
    "Applied crop x=%1 y=%2 width=%3 height=%4")
    .arg(appliedCrop_->x(), 0, 'f', 3)
    .arg(appliedCrop_->y(), 0, 'f', 3)
    .arg(appliedCrop_->width(), 0, 'f', 3)
    .arg(appliedCrop_->height(), 0, 'f', 3));
}

void MainWindow::clearCrop()
{
  appliedCrop_.reset();
  cropOverlay_->setAppliedCrop(std::nullopt);
  cropOverlay_->clearPendingCrop();
  updateCropUi();
  refreshSheetPreview();
  appendStatusMessage("Cleared crop.");
}

void MainWindow::updateCropUi()
{
  const bool hasPendingCrop = cropOverlay_->pendingCrop().has_value();
  const bool hasAppliedCrop = appliedCrop_.has_value();
  const bool canSelectCrop = projectInfo_.valid && projectInfo_.width > 0 && projectInfo_.height > 0;

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
    const QRectF crop = *appliedCrop_;
    cropStatusLabel_->setText(QStringLiteral(
      "Applied crop %1% x %2%")
      .arg(crop.width() * 100.0, 0, 'f', 1)
      .arg(crop.height() * 100.0, 0, 'f', 1));
    return;
  }

  cropStatusLabel_->setText("No crop");
}

void MainWindow::updateMetadata(const ProjectInfo& info)
{
  projectInfo_ = info;
  titleLabel_->setText(info.displayName.isEmpty() ? "No video loaded" : info.displayName);
  infoLabel_->setText(QStringLiteral("%1 ms  |  %2x%3  |  %4 fps  |  %5 frames")
    .arg(info.durationMs)
    .arg(info.width)
    .arg(info.height)
    .arg(info.fps, 0, 'f', 2)
    .arg(info.frameCount));
  backendLabel_->setText(QStringLiteral("Rust core bridge: %1")
    .arg(rustBridge_.cliPath().isEmpty() ? "unavailable" : rustBridge_.cliPath()));

  timelineWidget_->setFramesPerSecond(info.fps);
  timelineWidget_->setDurationMs(info.durationMs);
  cropOverlay_->setSourceVideoSize(QSize(info.width, info.height));
  updateCropUi();
  updateTransport(0, info.durationMs);
}

void MainWindow::updateTransport(qint64 positionMs, qint64 durationMs)
{
  timelineWidget_->setDurationMs(durationMs);
  timelineWidget_->setPositionMs(positionMs);

  timeLabel_->setText(QStringLiteral("%1 / %2").arg(formatTime(positionMs), formatTime(durationMs)));
  frameLabel_->setText(QStringLiteral("Frame %1").arg(displayFrameNumber(positionMs)));
}

void MainWindow::applyDarkPalette()
{
  qApp->setStyle("Fusion");
  setStyleSheet(R"(
    QMainWindow, QWidget {
      background: #10161d;
      color: #f4f7fb;
      font-family: "IBM Plex Sans", "Segoe UI", sans-serif;
      font-size: 14px;
    }
    QGroupBox {
      border: 1px solid #273647;
      border-radius: 10px;
      margin-top: 10px;
      padding: 14px 12px 12px 12px;
      background: #16202a;
      font-weight: 600;
    }
    QGroupBox::title {
      subcontrol-origin: margin;
      left: 12px;
      padding: 0 4px;
    }
    QPushButton, QSpinBox, QDoubleSpinBox {
      min-height: 32px;
    }
    QPushButton {
      border: 1px solid #30465c;
      border-radius: 8px;
      background: #203142;
      padding: 6px 12px;
    }
    QPushButton:hover {
      background: #294055;
    }
    QTextEdit {
      border: 1px solid #273647;
      border-radius: 8px;
      background: #0f151c;
    }
    QLabel#EyebrowLabel {
      color: #f0d46b;
      letter-spacing: 2px;
      text-transform: uppercase;
      font-size: 11px;
    }
    QLabel#SectionTitle {
      font-family: "Space Grotesk", "IBM Plex Sans", sans-serif;
      font-size: 24px;
      font-weight: 700;
    }
    QLabel#SectionDescription {
      color: #aab8c4;
    }
  )");
}

void MainWindow::createUi()
{
  auto* central = new QWidget;
  auto* outerLayout = new QVBoxLayout(central);
  outerLayout->setContentsMargins(12, 12, 12, 12);
  outerLayout->setSpacing(12);

  auto* splitter = new QSplitter(Qt::Horizontal, central);
  splitter->setChildrenCollapsible(false);

  auto* transportPane = new QWidget(splitter);
  auto* transportLayout = new QVBoxLayout(transportPane);
  transportLayout->setContentsMargins(0, 0, 0, 0);
  transportLayout->setSpacing(12);

  transportLayout->addWidget(makeSectionHeader(
    "NATIVE TRANSPORT",
    "Frame-accurate playback",
    "The transport pane is now backed directly by libmpv so playback, scrubbing, and frame stepping use one authoritative decoder state."));

  auto* headerRow = new QHBoxLayout;
  titleLabel_ = new QLabel("No video loaded");
  titleLabel_->setObjectName("SectionTitle");
  infoLabel_ = new QLabel("Open a local video to start the native shell migration.");
  infoLabel_->setStyleSheet("color: #aab8c4;");
  headerRow->addWidget(titleLabel_, 1);
  headerRow->addWidget(infoLabel_, 1);
  transportLayout->addLayout(headerRow);

  auto* playerHost = new QWidget(transportPane);
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
  controlsRow->addWidget(playButton);
  controlsRow->addWidget(pauseButton);
  controlsRow->addWidget(stopButton);
  controlsRow->addSpacing(12);
  controlsRow->addWidget(frameBackButton);
  controlsRow->addWidget(frameForwardButton);
  controlsRow->addStretch(1);
  transportLayout->addLayout(controlsRow);

  timelineWidget_ = new TimelineWidget(transportPane);
  transportLayout->addWidget(timelineWidget_);

  auto* metaRow = new QHBoxLayout;
  timeLabel_ = new QLabel("0:00.000 / 0:00.000");
  frameLabel_ = new QLabel("Frame 1");
  metaRow->addWidget(timeLabel_);
  metaRow->addStretch(1);
  metaRow->addWidget(frameLabel_);
  transportLayout->addLayout(metaRow);

  auto* cropRow = new QHBoxLayout;
  selectCropButton_ = new QPushButton("Select Crop");
  applyCropButton_ = new QPushButton("Apply");
  clearCropButton_ = new QPushButton("Clear");
  cropStatusLabel_ = new QLabel("No crop");
  selectCropButton_->setEnabled(false);
  applyCropButton_->setEnabled(false);
  clearCropButton_->setEnabled(false);
  cropStatusLabel_->setStyleSheet("color: #aab8c4;");
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

  auto* workspacePane = new QWidget(splitter);
  auto* workspaceLayout = new QVBoxLayout(workspacePane);
  workspaceLayout->setContentsMargins(0, 0, 0, 0);
  workspaceLayout->setSpacing(12);

  auto* sheetBox = new QGroupBox("Sheet Canvas", workspacePane);
  auto* sheetLayout = new QVBoxLayout(sheetBox);
  sheetLayout->addWidget(makeSectionHeader(
    "RUST RENDERER",
    "Rust-rendered starter sheet",
    "The right pane now shows a real starter-sheet preview rendered by the Rust backend for the currently loaded video."));

  sheetPreviewLabel_ = new QLabel(
    "Load a video to render the starter sheet preview.\n\n"
    "This is the first bridge between the native shell and the Rust renderer.");
  sheetPreviewLabel_->setWordWrap(true);
  sheetPreviewLabel_->setAlignment(Qt::AlignCenter);
  sheetPreviewLabel_->setMinimumHeight(280);
  sheetPreviewLabel_->setStyleSheet("background: #0d1319; border: 1px solid #273647; border-radius: 10px; color: #aab8c4; padding: 24px;");
  sheetLayout->addWidget(sheetPreviewLabel_, 1);
  workspaceLayout->addWidget(sheetBox, 1);

  auto* inspectorBox = new QGroupBox("Inspector / Migration Status", workspacePane);
  auto* inspectorLayout = new QVBoxLayout(inspectorBox);

  auto* summaryForm = new QFormLayout;
  backendLabel_ = new QLabel("Rust core bridge: unavailable");
  summaryForm->addRow("Backend", backendLabel_);
  auto* modeLabel = new QLabel("Qt 6 Widgets + libmpv transport");
  summaryForm->addRow("Desktop shell", modeLabel);
  auto* projectLabel = new QLabel("Rust CLI bridge for probe/export during migration");
  projectLabel->setWordWrap(true);
  summaryForm->addRow("Project bridge", projectLabel);
  auto* cropHelpLabel = new QLabel("Use Select Crop, drag on the player, then Apply.");
  cropHelpLabel->setWordWrap(true);
  summaryForm->addRow("Crop workflow", cropHelpLabel);
  inspectorLayout->addLayout(summaryForm);

  statusText_ = new QTextEdit(inspectorBox);
  statusText_->setReadOnly(true);
  statusText_->setMinimumHeight(180);
  statusText_->setPlainText(
    "Migration notes:\n"
    "- Native transport is authoritative.\n"
    "- Rust core stays responsible for probe, validation, rendering, and export.\n"
    "- Tauri remains in the repository only as a transition reference.\n");
  inspectorLayout->addWidget(statusText_, 1);
  workspaceLayout->addWidget(inspectorBox, 0);

  splitter->addWidget(transportPane);
  splitter->addWidget(workspacePane);
  splitter->setSizes({ width() / 2, width() / 2 });

  outerLayout->addWidget(splitter, 1);
  setCentralWidget(central);

  connect(playButton, &QPushButton::clicked, mpvWidget_, &MpvWidget::play);
  connect(pauseButton, &QPushButton::clicked, mpvWidget_, &MpvWidget::pause);
  connect(stopButton, &QPushButton::clicked, mpvWidget_, &MpvWidget::stopPlayback);
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
    updateTransport(positionMs, qMax<qint64>(timelineWidget_->durationMs(), mpvWidget_->durationMs()));
    mpvWidget_->seekPreviewMs(positionMs);
  });
  connect(timelineWidget_, &TimelineWidget::scrubFinished, this, [this](qint64 positionMs) {
    mpvWidget_->seekAbsoluteMs(positionMs);
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
}

void MainWindow::createMenuBar()
{
  auto* fileMenu = menuBar()->addMenu("&File");
  auto* openAction = fileMenu->addAction("&Open Video...");
  openAction->setShortcut(QKeySequence::Open);
  connect(openAction, &QAction::triggered, this, &MainWindow::openVideo);

  fileMenu->addSeparator();
  auto* quitAction = fileMenu->addAction("&Quit");
  quitAction->setShortcut(QKeySequence::Quit);
  connect(quitAction, &QAction::triggered, this, &QWidget::close);

  auto* playbackMenu = menuBar()->addMenu("&Playback");
  auto* playAction = playbackMenu->addAction("&Play");
  playAction->setShortcut(Qt::Key_Space);
  connect(playAction, &QAction::triggered, mpvWidget_, &MpvWidget::play);

  auto* pauseAction = playbackMenu->addAction("P&ause");
  connect(pauseAction, &QAction::triggered, mpvWidget_, &MpvWidget::pause);

  auto* stopAction = playbackMenu->addAction("&Stop");
  connect(stopAction, &QAction::triggered, mpvWidget_, &MpvWidget::stopPlayback);

  playbackMenu->addSeparator();
  auto* stepBackAction = playbackMenu->addAction("Step &Backward");
  stepBackAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Left));
  connect(stepBackAction, &QAction::triggered, this, [this] {
    mpvWidget_->stepFrames(-1, frameStepSpin_->value());
  });

  auto* stepForwardAction = playbackMenu->addAction("Step &Forward");
  stepForwardAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Right));
  connect(stepForwardAction, &QAction::triggered, this, [this] {
    mpvWidget_->stepFrames(1, frameStepSpin_->value());
  });
}

void MainWindow::appendStatusMessage(const QString& message)
{
  statusText_->append(message);
}

void MainWindow::refreshSheetPreview()
{
  beginPreviewRender();
}

void MainWindow::showSheetPreview(const QString& imagePath)
{
  sheetPreviewPath_ = imagePath;
  QPixmap pixmap(imagePath);
  if (pixmap.isNull()) {
    sheetPreviewLabel_->setText(QStringLiteral("Failed to load rendered preview image %1").arg(imagePath));
    return;
  }

  sheetPreviewLabel_->setText(QString());
  sheetPreviewLabel_->setPixmap(
    pixmap.scaled(900, 620, Qt::KeepAspectRatio, Qt::SmoothTransformation));
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
