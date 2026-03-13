#pragma once

#include <QString>
#include <QVector>

/**
 * Layout-only tile description shared between preview overlays and preset editing.
 */
struct SheetTileLayoutState {
  QString id;
  int order = 0;
  int row = 0;
  int column = 0;
  int rowSpan = 1;
  int columnSpan = 1;
};

/**
 * Render-relevant subset of a sheet layout used by the native overlay widgets.
 */
struct SheetLayoutPreviewState {
  int rows = 4;
  int columns = 5;
  int gutterPx = 12;
  int outerMarginPx = 24;
  bool showMetadataBar = true;
  double exportScale = 1.0;
  double sourceAspectRatio = 16.0 / 9.0;
  QVector<SheetTileLayoutState> tiles;
};
