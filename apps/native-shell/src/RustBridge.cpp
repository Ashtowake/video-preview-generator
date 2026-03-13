#include "RustBridge.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
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

QString cacheDirectoryPath(QString* errorMessage)
{
  const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  if (cacheRoot.isEmpty()) {
    if (errorMessage) {
      *errorMessage = "Failed to resolve cache directory.";
    }
    return {};
  }

  QDir cacheDir(cacheRoot);
  if (!cacheDir.exists() && !cacheDir.mkpath(".")) {
    if (errorMessage) {
      *errorMessage = "Failed to create cache directory.";
    }
    return {};
  }

  return cacheDir.absolutePath();
}

QString writeCachedJsonFile(
  const QString& cacheDirPath,
  const QString& prefix,
  const QJsonObject& object,
  QString* errorMessage)
{
  const QByteArray jsonBytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
  const QString fileName = QStringLiteral("%1-%2.json")
    .arg(prefix)
    .arg(QString::fromLatin1(QCryptographicHash::hash(jsonBytes, QCryptographicHash::Sha1).toHex()));
  const QString filePath = QDir(cacheDirPath).filePath(fileName);
  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to write cached JSON file %1").arg(filePath);
    }
    return {};
  }

  file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
  file.close();
  return filePath;
}

QJsonObject readJsonObjectFromFile(const QString& path, QString* errorMessage)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to open JSON file %1").arg(path);
    }
    return {};
  }

  const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
  if (!document.isObject()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("JSON file %1 did not contain an object").arg(path);
    }
    return {};
  }

  return document.object();
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
  int maxWidth,
  const std::optional<QRectF>& crop,
  std::optional<qint64> rangeStartMs,
  std::optional<qint64> rangeEndMs,
  std::optional<qint64> samplingStartMs) const
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

  const QByteArray cropCacheFragment = crop.has_value()
    ? QStringLiteral("%1,%2,%3,%4")
        .arg(crop->x(), 0, 'f', 6)
        .arg(crop->y(), 0, 'f', 6)
        .arg(crop->width(), 0, 'f', 6)
        .arg(crop->height(), 0, 'f', 6)
        .toUtf8()
    : QByteArray("none");
  const QByteArray rangeCacheFragment = (rangeStartMs.has_value() && rangeEndMs.has_value())
    ? QStringLiteral("%1,%2").arg(*rangeStartMs).arg(*rangeEndMs).toUtf8()
    : QByteArray("full");
  const QByteArray sampleStartCacheFragment = samplingStartMs.has_value()
    ? QByteArray::number(*samplingStartMs)
    : QByteArray("range-start");

  const QByteArray cacheKey = sourceInfo.absoluteFilePath().toUtf8()
    + '|'
    + QByteArray::number(sourceInfo.lastModified().toMSecsSinceEpoch())
    + '|'
    + QByteArray::number(maxWidth)
    + '|'
    + cropCacheFragment
    + '|'
    + rangeCacheFragment
    + '|'
    + sampleStartCacheFragment;
  const QString previewFileName = QStringLiteral("starter-preview-%1.png")
    .arg(QString::fromLatin1(QCryptographicHash::hash(cacheKey, QCryptographicHash::Sha1).toHex()));
  const QString previewPath = cacheDir.filePath(previewFileName);
  if (QFileInfo::exists(previewPath)) {
    return previewPath;
  }

  QProcess process;
  process.setWorkingDirectory(repoRoot());

  QStringList previewArguments{
    videoPath,
    "--out",
    previewPath,
    "--max-width",
    QString::number(maxWidth),
  };
  if (crop.has_value()) {
    previewArguments.append({
      "--crop-x",
      QString::number(crop->x(), 'f', 6),
      "--crop-y",
      QString::number(crop->y(), 'f', 6),
      "--crop-width",
      QString::number(crop->width(), 'f', 6),
      "--crop-height",
      QString::number(crop->height(), 'f', 6),
    });
  }
  if (rangeStartMs.has_value() && rangeEndMs.has_value()) {
    previewArguments.append({
      "--range-start",
      QString::number(*rangeStartMs),
      "--range-end",
      QString::number(*rangeEndMs),
    });
  }
  if (samplingStartMs.has_value()) {
    previewArguments.append({
      "--sample-start",
      QString::number(*samplingStartMs),
    });
  }

  const QStringList invocation = cliInvocation("preview", previewArguments);
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

QString RustBridge::renderTimelineStrip(
  const QString& videoPath,
  QString* errorMessage,
  int thumbnailCount,
  int targetWidth,
  int targetHeight,
  const std::optional<QRectF>& crop) const
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
      *errorMessage = "Failed to resolve cache directory for timeline thumbnails.";
    }
    return {};
  }

  QDir cacheDir(cacheRoot);
  if (!cacheDir.exists() && !cacheDir.mkpath(".")) {
    if (errorMessage) {
      *errorMessage = "Failed to create timeline thumbnail cache directory.";
    }
    return {};
  }

  const QByteArray cropCacheFragment = crop.has_value()
    ? QStringLiteral("%1,%2,%3,%4")
        .arg(crop->x(), 0, 'f', 6)
        .arg(crop->y(), 0, 'f', 6)
        .arg(crop->width(), 0, 'f', 6)
        .arg(crop->height(), 0, 'f', 6)
        .toUtf8()
    : QByteArray("none");
  const QByteArray cacheKey = sourceInfo.absoluteFilePath().toUtf8()
    + '|'
    + QByteArray::number(sourceInfo.lastModified().toMSecsSinceEpoch())
    + '|'
    + QByteArray::number(thumbnailCount)
    + '|'
    + QByteArray::number(targetWidth)
    + '|'
    + QByteArray::number(targetHeight)
    + '|'
    + cropCacheFragment;
  const QString stripFileName = QStringLiteral("timeline-strip-%1.png")
    .arg(QString::fromLatin1(QCryptographicHash::hash(cacheKey, QCryptographicHash::Sha1).toHex()));
  const QString stripPath = cacheDir.filePath(stripFileName);
  if (QFileInfo::exists(stripPath)) {
    return stripPath;
  }

  QStringList stripArguments{
    videoPath,
    "--out",
    stripPath,
    "--count",
    QString::number(thumbnailCount),
    "--width",
    QString::number(targetWidth),
    "--height",
    QString::number(targetHeight),
  };
  if (crop.has_value()) {
    stripArguments.append({
      "--crop-x",
      QString::number(crop->x(), 'f', 6),
      "--crop-y",
      QString::number(crop->y(), 'f', 6),
      "--crop-width",
      QString::number(crop->width(), 'f', 6),
      "--crop-height",
      QString::number(crop->height(), 'f', 6),
    });
  }

  QProcess process;
  process.setWorkingDirectory(repoRoot());

  const QStringList invocation = cliInvocation("timeline-strip", stripArguments);
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

  if (!QFileInfo::exists(stripPath)) {
    if (errorMessage) {
      *errorMessage = "Rust CLI timeline strip command did not produce an image.";
    }
    return {};
  }

  return stripPath;
}

QString RustBridge::renderProjectPreview(
  const QJsonObject& project,
  QString* errorMessage,
  int maxWidth) const
{
  QString cacheError;
  const QString cacheDirPath = cacheDirectoryPath(&cacheError);
  if (cacheDirPath.isEmpty()) {
    if (errorMessage) {
      *errorMessage = cacheError;
    }
    return {};
  }

  const QString projectPath = writeCachedJsonFile(cacheDirPath, "native-project-preview", project, errorMessage);
  if (projectPath.isEmpty()) {
    return {};
  }

  const QByteArray cacheKey = QFileInfo(projectPath).fileName().toUtf8() + '|' + QByteArray::number(maxWidth);
  const QString previewPath = QDir(cacheDirPath).filePath(QStringLiteral("project-preview-%1.png")
    .arg(QString::fromLatin1(QCryptographicHash::hash(cacheKey, QCryptographicHash::Sha1).toHex())));
  if (QFileInfo::exists(previewPath)) {
    return previewPath;
  }

  QProcess process;
  process.setWorkingDirectory(repoRoot());
  const QStringList invocation = cliInvocation(
    "preview-project",
    {
      projectPath,
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

  if (!runCliProcess(process, invocation.first(), invocation.mid(1), 30'000, errorMessage)) {
    return {};
  }

  return QFileInfo::exists(previewPath) ? previewPath : QString();
}

QJsonObject RustBridge::findSharpestNeighbours(
  const QJsonObject& project,
  const QStringList& tileIds,
  QString* errorMessage) const
{
  if (tileIds.isEmpty()) {
    if (errorMessage) {
      *errorMessage = "At least one tile id is required to search for sharpest neighbours.";
    }
    return {};
  }

  QString cacheError;
  const QString cacheDirPath = cacheDirectoryPath(&cacheError);
  if (cacheDirPath.isEmpty()) {
    if (errorMessage) {
      *errorMessage = cacheError;
    }
    return {};
  }

  const QString projectPath = writeCachedJsonFile(cacheDirPath, "native-project-sharpest-input", project, errorMessage);
  if (projectPath.isEmpty()) {
    return {};
  }

  const QByteArray outputKey = QFileInfo(projectPath).fileName().toUtf8() + '|' + tileIds.join('|').toUtf8();
  const QString outputPath = QDir(cacheDirPath).filePath(QStringLiteral("sharpest-project-%1.json")
    .arg(QString::fromLatin1(QCryptographicHash::hash(outputKey, QCryptographicHash::Sha1).toHex())));

  QStringList arguments{ projectPath, "--out", outputPath };
  for (const QString& tileId : tileIds) {
    arguments.append(tileId);
  }

  QProcess process;
  process.setWorkingDirectory(repoRoot());
  const QStringList invocation = cliInvocation("sharpest", arguments);
  if (invocation.isEmpty()) {
    if (errorMessage) {
      *errorMessage = "Rust CLI executable is not available.";
    }
    return {};
  }

  if (!runCliProcess(process, invocation.first(), invocation.mid(1), 30'000, errorMessage)) {
    return {};
  }

  return readJsonObjectFromFile(outputPath, errorMessage);
}

QString RustBridge::exportProject(
  const QJsonObject& project,
  const QString& outputPath,
  QString* errorMessage) const
{
  QString cacheError;
  const QString cacheDirPath = cacheDirectoryPath(&cacheError);
  if (cacheDirPath.isEmpty()) {
    if (errorMessage) {
      *errorMessage = cacheError;
    }
    return {};
  }

  const QString projectPath = writeCachedJsonFile(cacheDirPath, "native-project-export", project, errorMessage);
  if (projectPath.isEmpty()) {
    return {};
  }

  QProcess process;
  process.setWorkingDirectory(repoRoot());
  const QStringList invocation = cliInvocation("export", { projectPath, "--out", outputPath });
  if (invocation.isEmpty()) {
    if (errorMessage) {
      *errorMessage = "Rust CLI executable is not available.";
    }
    return {};
  }

  if (!runCliProcess(process, invocation.first(), invocation.mid(1), 60'000, errorMessage)) {
    return {};
  }

  return QFileInfo::exists(outputPath) ? outputPath : QString();
}

QString RustBridge::cliPath() const
{
  const QString explicitPath = qEnvironmentVariable("VPG_CLI_PATH");
  if (!explicitPath.isEmpty() && QFileInfo::exists(explicitPath)) {
    return explicitPath;
  }

  const QString applicationDir = QCoreApplication::applicationDirPath();
  const QStringList candidates{
    QDir(applicationDir).filePath(QStringLiteral("vpg-cli%1").arg(executableSuffix())),
    QDir(applicationDir).filePath(QStringLiteral("video-preview%1").arg(executableSuffix())),
    QDir(repoRoot()).filePath(QStringLiteral("target/debug/vpg-cli%1").arg(executableSuffix())),
    QDir(repoRoot()).filePath(QStringLiteral("target/release/vpg-cli%1").arg(executableSuffix())),
  };

  for (const QString& candidate : candidates) {
    if (QFileInfo::exists(candidate)) {
      return candidate;
    }
  }

  for (const QString& binaryName : { QStringLiteral("vpg-cli%1").arg(executableSuffix()), QStringLiteral("video-preview%1").arg(executableSuffix()) }) {
    const QString resolved = QStandardPaths::findExecutable(binaryName);
    if (!resolved.isEmpty()) {
      return resolved;
    }
  }

  return {};
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
