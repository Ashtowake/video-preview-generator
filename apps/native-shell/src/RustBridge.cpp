#include "RustBridge.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>

namespace {

QString executableSuffix()
{
#ifdef Q_OS_WIN
  return ".exe";
#else
  return "";
#endif
}

qint64 jsonInteger(const QJsonObject& object, const char* key)
{
  return object.value(QLatin1String(key)).toVariant().toLongLong();
}

double jsonDouble(const QJsonObject& object, const char* key)
{
  return object.value(QLatin1String(key)).toDouble();
}

bool runCliProcess(
  QProcess& process,
  const QString& program,
  const QStringList& arguments,
  int timeoutMs,
  QString* errorMessage)
{
  process.start(program, arguments);
  if (!process.waitForStarted()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to launch Rust CLI: %1").arg(program);
    }
    return false;
  }

  if (!process.waitForFinished(timeoutMs)) {
    process.kill();
    process.waitForFinished();
    if (errorMessage) {
      *errorMessage = "Rust CLI command timed out.";
    }
    return false;
  }

  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    if (errorMessage) {
      const QString stderrText = QString::fromUtf8(process.readAllStandardError()).trimmed();
      *errorMessage = stderrText.isEmpty()
        ? "Rust CLI command failed."
        : stderrText;
    }
    return false;
  }

  return true;
}

} // namespace

RustBridge::RustBridge(QObject* parent)
  : QObject(parent)
{
}

ProjectInfo RustBridge::inspectVideo(const QString& videoPath, QString* errorMessage) const
{
  QProcess process;
  process.setWorkingDirectory(repoRoot());

  const QStringList invocation = cliInvocation("inspect", { videoPath });
  if (invocation.isEmpty()) {
    if (errorMessage) {
      *errorMessage = "Rust CLI executable is not available.";
    }
    return {};
  }

  const QString program = invocation.first();
  const QStringList arguments = invocation.mid(1);

  if (!runCliProcess(process, program, arguments, 15'000, errorMessage)) {
    return {};
  }

  const QJsonDocument document = QJsonDocument::fromJson(process.readAllStandardOutput());
  if (!document.isObject()) {
    if (errorMessage) {
      *errorMessage = "Rust CLI returned invalid JSON.";
    }
    return {};
  }

  const QJsonObject video = document.object().value("video").toObject();
  ProjectInfo info;
  info.videoPath = videoPath;
  info.displayName = QFileInfo(videoPath).fileName();
  info.durationMs = jsonInteger(video, "durationMs");
  info.fps = jsonDouble(video, "fps");
  info.frameCount = jsonInteger(video, "frameCount");
  info.width = static_cast<int>(jsonInteger(video, "width"));
  info.height = static_cast<int>(jsonInteger(video, "height"));
  info.valid = !info.displayName.isEmpty();
  return info;
}

QString RustBridge::renderStarterPreview(
  const QString& videoPath,
  QString* errorMessage,
  int maxWidth) const
{
  const QFileInfo sourceInfo(videoPath);
  if (!sourceInfo.exists()) {
    if (errorMessage) {
      *errorMessage = "Video file does not exist.";
    }
    return {};
  }

  const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  if (cacheRoot.isEmpty()) {
    if (errorMessage) {
      *errorMessage = "Failed to resolve cache directory for preview rendering.";
    }
    return {};
  }

  QDir cacheDir(cacheRoot);
  if (!cacheDir.exists() && !cacheDir.mkpath(".")) {
    if (errorMessage) {
      *errorMessage = "Failed to create preview cache directory.";
    }
    return {};
  }

  const QByteArray cacheKey = sourceInfo.absoluteFilePath().toUtf8()
    + '|'
    + QByteArray::number(sourceInfo.lastModified().toMSecsSinceEpoch())
    + '|'
    + QByteArray::number(maxWidth);
  const QString previewFileName = QStringLiteral("starter-preview-%1.png")
    .arg(QString::fromLatin1(QCryptographicHash::hash(cacheKey, QCryptographicHash::Sha1).toHex()));
  const QString previewPath = cacheDir.filePath(previewFileName);

  QProcess process;
  process.setWorkingDirectory(repoRoot());

  const QStringList invocation = cliInvocation("preview", {
    videoPath,
    "--out",
    previewPath,
    "--max-width",
    QString::number(maxWidth),
  });
  if (invocation.isEmpty()) {
    if (errorMessage) {
      *errorMessage = "Rust CLI executable is not available.";
    }
    return {};
  }

  const QString program = invocation.first();
  const QStringList arguments = invocation.mid(1);
  if (!runCliProcess(process, program, arguments, 30'000, errorMessage)) {
    return {};
  }

  if (!QFileInfo::exists(previewPath)) {
    if (errorMessage) {
      *errorMessage = "Rust CLI preview command did not produce an image.";
    }
    return {};
  }

  return previewPath;
}

QString RustBridge::cliPath() const
{
  const QString explicitPath = qEnvironmentVariable("VPG_CLI_PATH");
  if (!explicitPath.isEmpty() && QFileInfo::exists(explicitPath)) {
    return explicitPath;
  }

  const QString repoBinary = QDir(repoRoot()).filePath(QStringLiteral("target/debug/vpg-cli%1").arg(executableSuffix()));
  if (QFileInfo::exists(repoBinary)) {
    return repoBinary;
  }

  return QStandardPaths::findExecutable("video-preview");
}

QString RustBridge::repoRoot() const
{
  return QStringLiteral(VPG_REPO_ROOT);
}

QStringList RustBridge::cliInvocation(const QString& command, const QStringList& arguments) const
{
  const QString cli = cliPath();
  if (!cli.isEmpty()) {
    QStringList invocation{ cli, command };
    invocation.append(arguments);
    return invocation;
  }

  const QString cargo = QStandardPaths::findExecutable("cargo");
  if (cargo.isEmpty()) {
    return {};
  }

  QStringList invocation{
    cargo,
    "run",
    "-q",
    "-p",
    "vpg-cli",
    "--",
    command,
  };
  invocation.append(arguments);
  return invocation;
}
