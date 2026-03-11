#pragma once

#include <QObject>

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
  [[nodiscard]] QString cliPath() const;

private:
  [[nodiscard]] QString repoRoot() const;
  [[nodiscard]] QStringList cliInvocation(const QString& command, const QString& argument) const;
};
