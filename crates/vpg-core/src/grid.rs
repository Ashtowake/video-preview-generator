use crate::project::{ProjectTile, TileSelection, TileSpan};
use crate::seek::center_of_bin_samples;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct GridValidationIssue {
    pub tile_id: String,
    pub reason: String,
}

/// Assigns evenly spaced automatic tile timestamps to unpinned tiles only.
pub fn assign_auto_tiles(
    tiles: &mut [ProjectTile],
    range_start_ms: u64,
    range_end_ms: u64,
    fps: f64,
) {
    let auto_indices: Vec<usize> = tiles
        .iter()
        .enumerate()
        .filter_map(|(index, tile)| (!tile.pinned).then_some(index))
        .collect();

    let samples = center_of_bin_samples(range_start_ms, range_end_ms, auto_indices.len());

    for (sample_index, tile_index) in auto_indices.into_iter().enumerate() {
        let sample_ms = samples[sample_index];
        let frame_index = if fps > 0.0 {
            ((sample_ms as f64 / 1000.0) * fps).round() as u64
        } else {
            0
        };

        tiles[tile_index].selection = TileSelection::ManualFrame {
            frame_index,
            time_ms: sample_ms,
        };
        tiles[tile_index].fine_tune_offset_ms = 0;
    }
}

/// Validates grid spans against sheet bounds and overlap.
pub fn validate_grid_spans(
    tiles: &[ProjectTile],
    rows: u32,
    columns: u32,
) -> Vec<GridValidationIssue> {
    let mut occupied = vec![vec![None::<String>; columns as usize]; rows as usize];
    let mut issues = Vec::new();

    for tile in tiles {
        let TileSpan {
            row,
            column,
            row_span,
            column_span,
        } = tile.span;

        if row + row_span > rows
            || column + column_span > columns
            || row_span == 0
            || column_span == 0
        {
            issues.push(GridValidationIssue {
                tile_id: tile.id.clone(),
                reason: "tile span falls outside the grid bounds".to_string(),
            });
            continue;
        }

        for current_row in row..row + row_span {
            for current_column in column..column + column_span {
                let slot = &mut occupied[current_row as usize][current_column as usize];
                if let Some(existing_tile_id) = slot {
                    issues.push(GridValidationIssue {
                        tile_id: tile.id.clone(),
                        reason: format!("tile overlaps with {existing_tile_id}"),
                    });
                } else {
                    *slot = Some(tile.id.clone());
                }
            }
        }
    }

    issues
}

#[cfg(test)]
mod tests {
    use crate::project::{GridSettings, ProjectFile, TileSelection};

    use super::{assign_auto_tiles, validate_grid_spans};

    #[test]
    fn auto_assignment_skips_pinned_tiles() {
        let mut project = ProjectFile::starter("movie.mp4");
        project.tiles[0].pinned = true;
        project.tiles[0].selection = TileSelection::ManualFrame {
            frame_index: 10,
            time_ms: 500,
        };

        assign_auto_tiles(&mut project.tiles, 0, 10_000, 24.0);

        assert_eq!(
            project.tiles[0].selection,
            TileSelection::ManualFrame {
                frame_index: 10,
                time_ms: 500
            }
        );
        assert!(matches!(
            project.tiles[1].selection,
            TileSelection::ManualFrame { .. }
        ));
    }

    #[test]
    fn overlapping_tiles_are_reported() {
        let mut project = ProjectFile::starter("movie.mp4");
        project.grid = GridSettings {
            rows: 2,
            columns: 2,
            gutter_px: 8,
            outer_margin_px: 8,
            default_sharpness_window: 12,
        };
        project.tiles[0].span.row_span = 2;
        project.tiles[1].span.row = 1;

        let issues = validate_grid_spans(
            &project.tiles[0..2],
            project.grid.rows,
            project.grid.columns,
        );
        assert_eq!(issues.len(), 1);
        assert!(issues[0].reason.contains("overlaps"));
    }
}
