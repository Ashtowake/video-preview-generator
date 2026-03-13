#pragma once

#include <optional>

#include <QObject>
#include <QJsonObject>
#include <QRectF>

#include "ProjectInfo.hpp"

/**
 * Development-time bridge into the Rust backend.
 *
 * The Qt shell talks to the existing Rust CLI first so the migration can replace the shell/UI
 * layer without reimplementing probe/export logic in C++.
 */
class RustBridge final : public QObject {
  Q_OBJECT

public:
  explicit RustBridge(QObject* parent = nullptr);

  [[nodiscard]] ProjectInfo inspectVideo(const QString& videoPath, QString* errorMessage = nullptr) const;
  [[nodiscard]] QString renderStarterPreview(
    const QString& videoPath,
    QString* errorMessage = nullptr,
    int maxWidth = 1200,
    const std::optional<QRectF>& crop = std::nullopt,
    std::optional<qint64> rangeStartMs = std::nullopt,
    std::optional<qint64> rangeEndMs = std::nullopt,
    std::optional<qint64> samplingStartMs = std::nullopt) const;
  [[nodiscard]] QString renderTimelineStrip(
    const QString& videoPath,
    QString* errorMessage = nullptr,
    int thumbnailCount = 48,
    int targetWidth = 3456,
    int targetHeight = 84,
    const std::optional<QRectF>& crop = std::nullopt) const;
  [[nodiscard]] QString renderProjectPreview(
    const QJsonObject& project,
    QString* errorMessage = nullptr,
    int maxWidth = 1200) const;
  [[nodiscard]] QJsonObject findSharpestNeighbours(
    const QJsonObject& project,
    const QStringList& tileIds,
    QString* errorMessage = nullptr) const;
  [[nodiscard]] QString exportProject(
    const QJsonObject& project,
    const QString& outputPath,
    QString* errorMessage = nullptr) const;
  [[nodiscard]] QString cliPath() const;

private:
  [[nodiscard]] QString repoRoot() const;
  [[nodiscard]] QStringList cliInvocation(const QString& command, const QStringList& arguments) const;
};
