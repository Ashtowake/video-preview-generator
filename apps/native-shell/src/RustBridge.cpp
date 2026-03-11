#include "RustBridge.hpp"

#include <QCoreApplication>
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

} // namespace

RustBridge::RustBridge(QObject* parent)
  : QObject(parent)
{
}

ProjectInfo RustBridge::inspectVideo(const QString& videoPath, QString* errorMessage) const
{
  QProcess process;
  process.setWorkingDirectory(repoRoot());

  const QStringList invocation = cliInvocation("inspect", videoPath);
  if (invocation.isEmpty()) {
    if (errorMessage) {
      *errorMessage = "Rust CLI executable is not available.";
    }
    return {};
  }

  const QString program = invocation.first();
  const QStringList arguments = invocation.mid(1);

  process.start(program, arguments);
  if (!process.waitForStarted()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to launch Rust CLI: %1").arg(program);
    }
    return {};
  }

  if (!process.waitForFinished(15'000)) {
    process.kill();
    process.waitForFinished();
    if (errorMessage) {
      *errorMessage = "Rust CLI probe timed out.";
    }
    return {};
  }

  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    if (errorMessage) {
      const QString stderrText = QString::fromUtf8(process.readAllStandardError()).trimmed();
      *errorMessage = stderrText.isEmpty()
        ? "Rust CLI probe failed."
        : stderrText;
    }
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

QStringList RustBridge::cliInvocation(const QString& command, const QString& argument) const
{
  const QString cli = cliPath();
  if (!cli.isEmpty()) {
    return { cli, command, argument };
  }

  const QString cargo = QStandardPaths::findExecutable("cargo");
  if (cargo.isEmpty()) {
    return {};
  }

  return {
    cargo,
    "run",
    "-q",
    "-p",
    "vpg-cli",
    "--",
    command,
    argument,
  };
}
