#pragma once

#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QTimer>
#include <QShowEvent>

struct mpv_handle;
struct mpv_render_context;

/**
 * Native libmpv-backed transport surface.
 *
 * This widget owns the authoritative playback path so scrubbing, playback, and frame stepping
 * are driven by one decoder state instead of a proxy video plus exact-preview overlay.
 */
class MpvWidget final : public QOpenGLWidget, protected QOpenGLFunctions {
  Q_OBJECT

public:
  explicit MpvWidget(QWidget* parent = nullptr);
  ~MpvWidget() override;

  void loadFile(const QString& path);
  void play();
  void pause();
  void togglePause();
  void stopPlayback();
  void seekAbsoluteMs(qint64 positionMs);
  void seekPreviewMs(qint64 positionMs);
  void seekRelativeMs(qint64 deltaMs);
  void stepFrames(int direction, int count);

  [[nodiscard]] bool isPaused() const;
  [[nodiscard]] bool hasMedia() const;
  [[nodiscard]] qint64 currentTimeMs() const;
  [[nodiscard]] qint64 durationMs() const;

signals:
  void playbackStateChanged(bool paused);
  void positionChanged(qint64 positionMs, qint64 durationMs);
  void playerError(const QString& message);

protected:
  void initializeGL() override;
  void paintGL() override;
  void showEvent(QShowEvent* event) override;

private:
  static void onMpvUpdate(void* context);
  static void* getProcAddress(void* context, const char* name);

  void handleMpvUpdate();
  bool ensureInitialized();
  void dispatchPreviewSeek(qint64 positionMs);
  void initializePlayerCore();
  void initializeRenderContext();
  void destroyPlayer();
  void queueRender();
  void pollState();
  bool runCommand(const QStringList& arguments);
  bool setFlagProperty(const char* property, bool value);
  bool setDoubleProperty(const char* property, double value);
  [[nodiscard]] bool getFlagProperty(const char* property, bool fallback) const;
  [[nodiscard]] double getDoubleProperty(const char* property, double fallback) const;
  [[nodiscard]] qint64 clampedPosition(qint64 positionMs) const;

  mpv_handle* mpv_ = nullptr;
  mpv_render_context* renderContext_ = nullptr;
  QTimer statePollTimer_;
  QString pendingPath_;
  bool coreReady_ = false;
  bool hasMedia_ = false;
  bool paused_ = true;
  qint64 positionMs_ = 0;
  qint64 durationMs_ = 0;
  qint64 pendingPreviewSeekMs_ = -1;
  bool previewSeekInFlight_ = false;
};
