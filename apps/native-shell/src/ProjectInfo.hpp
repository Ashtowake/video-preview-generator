#pragma once

#include <QString>

/**
 * Minimal video metadata bridge shared between the Qt shell and the Rust CLI.
 *
 * The native shell starts with transport and metadata so playback can move to a native stack
 * without blocking on a full project-editor rewrite.
 */
struct ProjectInfo {
  QString videoPath;
  QString displayName;
  qint64 durationMs = 0;
  double fps = 0.0;
  qint64 frameCount = 0;
  int width = 0;
  int height = 0;
  bool valid = false;
};
