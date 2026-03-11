#include "MpvWidget.hpp"

#include <QMetaObject>
#include <QOpenGLContext>
#include <QOpenGLFunctions>

#include <mpv/client.h>
#include <mpv/render_gl.h>

namespace {

constexpr int kRenderFlipY = 1;
constexpr int kPollIntervalMs = 33;

QString mpvErrorString(int status)
{
  return QString::fromUtf8(mpv_error_string(status));
}

} // namespace

MpvWidget::MpvWidget(QWidget* parent)
  : QOpenGLWidget(parent)
{
  setUpdateBehavior(QOpenGLWidget::PartialUpdate);
  setMinimumHeight(360);

  statePollTimer_.setInterval(kPollIntervalMs);
  connect(&statePollTimer_, &QTimer::timeout, this, &MpvWidget::pollState);
}

MpvWidget::~MpvWidget()
{
  makeCurrent();
  destroyPlayer();
  doneCurrent();
}

void MpvWidget::loadFile(const QString& path)
{
  pendingPath_ = path;
  hasMedia_ = !path.isEmpty();

  if (!mpv_) {
    update();
    return;
  }

  if (!runCommand({ "loadfile", path, "replace" })) {
    return;
  }

  setFlagProperty("pause", true);
  paused_ = true;
  positionMs_ = 0;
  durationMs_ = 0;
  emit playbackStateChanged(true);
  emit positionChanged(positionMs_, durationMs_);
  statePollTimer_.start();
}

void MpvWidget::play()
{
  if (!mpv_) {
    return;
  }

  if (setFlagProperty("pause", false)) {
    paused_ = false;
    emit playbackStateChanged(false);
    statePollTimer_.start();
  }
}

void MpvWidget::pause()
{
  if (!mpv_) {
    return;
  }

  if (setFlagProperty("pause", true)) {
    paused_ = true;
    emit playbackStateChanged(true);
    pollState();
  }
}

void MpvWidget::togglePause()
{
  if (isPaused()) {
    play();
  } else {
    pause();
  }
}

void MpvWidget::stopPlayback()
{
  pause();
  seekAbsoluteMs(0);
}

void MpvWidget::seekAbsoluteMs(qint64 positionMs)
{
  if (!mpv_) {
    return;
  }

  const double seconds = static_cast<double>(clampedPosition(positionMs)) / 1000.0;
  if (setDoubleProperty("time-pos", seconds)) {
    pollState();
    queueRender();
  }
}

void MpvWidget::seekRelativeMs(qint64 deltaMs)
{
  seekAbsoluteMs(positionMs_ + deltaMs);
}

void MpvWidget::stepFrames(int direction, int count)
{
  if (!mpv_ || count <= 0) {
    return;
  }

  pause();
  const QString commandName = direction < 0 ? "frame-back-step" : "frame-step";
  for (int index = 0; index < count; ++index) {
    if (!runCommand({ commandName })) {
      break;
    }
  }
  pollState();
}

bool MpvWidget::isPaused() const
{
  return paused_;
}

bool MpvWidget::hasMedia() const
{
  return hasMedia_;
}

qint64 MpvWidget::currentTimeMs() const
{
  return positionMs_;
}

qint64 MpvWidget::durationMs() const
{
  return durationMs_;
}

void MpvWidget::initializeGL()
{
  initializeOpenGLFunctions();
  initializePlayer();
}

void MpvWidget::paintGL()
{
  glClearColor(0.03f, 0.05f, 0.08f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  if (!renderContext_) {
    return;
  }

  mpv_opengl_fbo framebufferObject {
    static_cast<int>(defaultFramebufferObject()),
    static_cast<int>(width() * devicePixelRatio()),
    static_cast<int>(height() * devicePixelRatio()),
    0,
  };

  mpv_render_param parameters[] = {
    { MPV_RENDER_PARAM_OPENGL_FBO, &framebufferObject },
    { MPV_RENDER_PARAM_FLIP_Y, const_cast<int*>(&kRenderFlipY) },
    { MPV_RENDER_PARAM_INVALID, nullptr },
  };

  mpv_render_context_render(renderContext_, parameters);
}

void MpvWidget::onMpvUpdate(void* context)
{
  auto* widget = static_cast<MpvWidget*>(context);
  QMetaObject::invokeMethod(widget, &MpvWidget::queueRender, Qt::QueuedConnection);
}

void* MpvWidget::getProcAddress(void* context, const char* name)
{
  Q_UNUSED(context);
  if (auto* currentContext = QOpenGLContext::currentContext()) {
    return reinterpret_cast<void*>(currentContext->getProcAddress(name));
  }

  return nullptr;
}

void MpvWidget::initializePlayer()
{
  if (mpv_) {
    return;
  }

  mpv_ = mpv_create();
  if (!mpv_) {
    emit playerError("Failed to create libmpv instance.");
    return;
  }

  mpv_set_option_string(mpv_, "config", "no");
  mpv_set_option_string(mpv_, "terminal", "no");
  mpv_set_option_string(mpv_, "msg-level", "all=warn");
  mpv_set_option_string(mpv_, "osc", "no");
  mpv_set_option_string(mpv_, "input-default-bindings", "no");
  mpv_set_option_string(mpv_, "input-vo-keyboard", "no");
  mpv_set_option_string(mpv_, "keep-open", "yes");
  mpv_set_option_string(mpv_, "pause", "yes");
  mpv_set_option_string(mpv_, "hwdec", "auto-safe");

  const int initStatus = mpv_initialize(mpv_);
  if (initStatus < 0) {
    emit playerError(QStringLiteral("Failed to initialize libmpv: %1").arg(mpvErrorString(initStatus)));
    destroyPlayer();
    return;
  }

  mpv_opengl_init_params initParameters {
    &MpvWidget::getProcAddress,
    this,
  };

  mpv_render_param renderParameters[] = {
    { MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL) },
    { MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &initParameters },
    { MPV_RENDER_PARAM_INVALID, nullptr },
  };

  const int renderStatus = mpv_render_context_create(&renderContext_, mpv_, renderParameters);
  if (renderStatus < 0) {
    emit playerError(QStringLiteral("Failed to create libmpv render context: %1").arg(mpvErrorString(renderStatus)));
    destroyPlayer();
    return;
  }

  mpv_render_context_set_update_callback(renderContext_, &MpvWidget::onMpvUpdate, this);

  if (!pendingPath_.isEmpty()) {
    loadFile(pendingPath_);
  }
}

void MpvWidget::destroyPlayer()
{
  statePollTimer_.stop();

  if (renderContext_) {
    mpv_render_context_free(renderContext_);
    renderContext_ = nullptr;
  }

  if (mpv_) {
    mpv_terminate_destroy(mpv_);
    mpv_ = nullptr;
  }
}

void MpvWidget::queueRender()
{
  update();
}

void MpvWidget::pollState()
{
  if (!mpv_) {
    return;
  }

  const bool nextPaused = getFlagProperty("pause", true);
  const qint64 nextPositionMs = static_cast<qint64>(getDoubleProperty("time-pos", 0.0) * 1000.0);
  const qint64 nextDurationMs = static_cast<qint64>(getDoubleProperty("duration", 0.0) * 1000.0);

  const bool pausedChanged = nextPaused != paused_;
  const bool positionChangedNow = nextPositionMs != positionMs_ || nextDurationMs != durationMs_;

  paused_ = nextPaused;
  positionMs_ = nextPositionMs;
  durationMs_ = nextDurationMs;

  if (pausedChanged) {
    emit playbackStateChanged(paused_);
  }

  if (positionChangedNow) {
    emit positionChanged(positionMs_, durationMs_);
  }

  if (paused_ && positionMs_ == 0 && durationMs_ == 0 && !pendingPath_.isEmpty()) {
    statePollTimer_.start();
    return;
  }

  if (paused_ && !hasMedia_) {
    statePollTimer_.stop();
  }
}

bool MpvWidget::runCommand(const QStringList& arguments)
{
  if (!mpv_) {
    return false;
  }

  QVector<QByteArray> utf8Arguments;
  utf8Arguments.reserve(arguments.size());
  QVector<const char*> rawArguments;
  rawArguments.reserve(arguments.size() + 1);

  for (const QString& argument : arguments) {
    utf8Arguments.push_back(argument.toUtf8());
    rawArguments.push_back(utf8Arguments.constLast().constData());
  }
  rawArguments.push_back(nullptr);

  const int status = mpv_command_async(mpv_, 0, rawArguments.data());
  if (status < 0) {
    emit playerError(QStringLiteral("libmpv command failed: %1").arg(mpvErrorString(status)));
    return false;
  }

  return true;
}

bool MpvWidget::setFlagProperty(const char* property, bool value)
{
  if (!mpv_) {
    return false;
  }

  int flag = value ? 1 : 0;
  const int status = mpv_set_property(mpv_, property, MPV_FORMAT_FLAG, &flag);
  if (status < 0) {
    emit playerError(QStringLiteral("Failed to set %1: %2")
      .arg(QString::fromUtf8(property), mpvErrorString(status)));
    return false;
  }

  return true;
}

bool MpvWidget::setDoubleProperty(const char* property, double value)
{
  if (!mpv_) {
    return false;
  }

  const int status = mpv_set_property(mpv_, property, MPV_FORMAT_DOUBLE, &value);
  if (status < 0) {
    emit playerError(QStringLiteral("Failed to set %1: %2")
      .arg(QString::fromUtf8(property), mpvErrorString(status)));
    return false;
  }

  return true;
}

bool MpvWidget::getFlagProperty(const char* property, bool fallback) const
{
  if (!mpv_) {
    return fallback;
  }

  int flag = fallback ? 1 : 0;
  if (mpv_get_property(mpv_, property, MPV_FORMAT_FLAG, &flag) < 0) {
    return fallback;
  }

  return flag != 0;
}

double MpvWidget::getDoubleProperty(const char* property, double fallback) const
{
  if (!mpv_) {
    return fallback;
  }

  double value = fallback;
  if (mpv_get_property(mpv_, property, MPV_FORMAT_DOUBLE, &value) < 0) {
    return fallback;
  }

  return value;
}

qint64 MpvWidget::clampedPosition(qint64 positionMs) const
{
  if (durationMs_ <= 0) {
    return qMax<qint64>(0, positionMs);
  }

  return qBound<qint64>(0, positionMs, durationMs_);
}
