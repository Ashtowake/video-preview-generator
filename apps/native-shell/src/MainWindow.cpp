#include "MainWindow.hpp"

#include <QAction>
#include <QApplication>
#include <cmath>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QSpinBox>
#include <functional>

#include "MpvWidget.hpp"

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

  QString errorMessage;
  const ProjectInfo info = rustBridge_.inspectVideo(path, &errorMessage);
  if (!info.valid) {
    appendStatusMessage(errorMessage.isEmpty() ? "Rust inspect bridge failed." : errorMessage);
  } else {
    updateMetadata(info);
    appendStatusMessage(QStringLiteral("Loaded %1 via Rust CLI metadata bridge.").arg(info.displayName));
    refreshSheetPreview();
  }

  mpvWidget_->loadFile(path);
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

  playheadSlider_->setMaximum(static_cast<int>(qMax<qint64>(0, info.durationMs)));
  updateTransport(0, info.durationMs);
}

void MainWindow::updateTransport(qint64 positionMs, qint64 durationMs)
{
  if (!scrubbing_) {
    playheadSlider_->blockSignals(true);
    playheadSlider_->setMaximum(static_cast<int>(qMax<qint64>(0, durationMs)));
    playheadSlider_->setValue(static_cast<int>(qBound<qint64>(0, positionMs, durationMs)));
    playheadSlider_->blockSignals(false);
  }

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
    QPushButton, QSpinBox, QDoubleSpinBox, QSlider {
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

  mpvWidget_ = new MpvWidget(transportPane);
  transportLayout->addWidget(mpvWidget_, 1);

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

  playheadSlider_ = new QSlider(Qt::Horizontal, transportPane);
  playheadSlider_->setRange(0, 0);
  transportLayout->addWidget(playheadSlider_);

  auto* metaRow = new QHBoxLayout;
  timeLabel_ = new QLabel("0:00.000 / 0:00.000");
  frameLabel_ = new QLabel("Frame 1");
  metaRow->addWidget(timeLabel_);
  metaRow->addStretch(1);
  metaRow->addWidget(frameLabel_);
  transportLayout->addLayout(metaRow);

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

  connect(playheadSlider_, &QSlider::sliderPressed, this, [this] {
    scrubbing_ = true;
  });
  connect(playheadSlider_, &QSlider::sliderReleased, this, [this] {
    scrubbing_ = false;
    mpvWidget_->seekAbsoluteMs(playheadSlider_->value());
  });
  connect(playheadSlider_, &QSlider::sliderMoved, this, [this](int value) {
    updateTransport(value, qMax<qint64>(playheadSlider_->maximum(), 0));
    mpvWidget_->seekAbsoluteMs(value);
  });

  connect(mpvWidget_, &MpvWidget::positionChanged, this, &MainWindow::updateTransport);
  connect(mpvWidget_, &MpvWidget::playerError, this, [this](const QString& message) {
    appendStatusMessage(message);
    statusBar()->showMessage(message, 5000);
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
  if (!projectInfo_.valid || projectInfo_.videoPath.isEmpty() || !sheetPreviewLabel_) {
    return;
  }

  sheetPreviewLabel_->setPixmap(QPixmap());
  sheetPreviewLabel_->setText("Rendering starter sheet preview...");

  QString errorMessage;
  const QString previewPath = rustBridge_.renderStarterPreview(projectInfo_.videoPath, &errorMessage, 1100);
  if (previewPath.isEmpty()) {
    sheetPreviewLabel_->setText(errorMessage.isEmpty()
      ? "Failed to render starter sheet preview."
      : errorMessage);
    appendStatusMessage(sheetPreviewLabel_->text());
    return;
  }

  showSheetPreview(previewPath);
  appendStatusMessage(QStringLiteral("Rendered starter sheet preview to %1").arg(previewPath));
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
