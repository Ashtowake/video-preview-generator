use serde::{Deserialize, Serialize};

/// Serialized project file shared by the desktop app and the CLI.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct ProjectFile {
    pub version: u32,
    pub analysis_mode: AnalysisMode,
    pub video: VideoSource,
    pub playback: PlaybackSettings,
    pub range: TimeRange,
    pub grid: GridSettings,
    pub tiles: Vec<ProjectTile>,
    pub style: ProjectStyle,
    pub watermark: WatermarkSettings,
    pub export: ExportSettings,
    pub batch: BatchSettings,
}

impl ProjectFile {
    /// Creates a sensible default project for a single imported video.
    pub fn starter(video_path: impl Into<String>) -> Self {
        let grid = GridSettings::default();
        let total_tiles = (grid.rows * grid.columns) as usize;
        let grid_columns = grid.columns;

        Self {
            version: 1,
            analysis_mode: AnalysisMode::QuickPreview,
            video: VideoSource {
                path: video_path.into(),
                duration_ms: Some(60_000),
                fps: Some(24.0),
                frame_count: Some(1_440),
                width: Some(1920),
                height: Some(1080),
                crop: None,
            },
            playback: PlaybackSettings::default(),
            range: TimeRange {
                start_ms: 0,
                end_ms: 60_000,
                sample_start_ms: 0,
            },
            grid,
            tiles: (0..total_tiles)
                .map(|index| {
                    let order = index as u32;
                    ProjectTile {
                        span: TileSpan {
                            row: order / grid_columns,
                            column: order % grid_columns,
                            row_span: 1,
                            column_span: 1,
                        },
                        ..ProjectTile::auto(order)
                    }
                })
                .collect(),
            style: ProjectStyle::default(),
            watermark: WatermarkSettings::default(),
            export: ExportSettings::default(),
            batch: BatchSettings::default(),
        }
    }
}

/// Reusable sheet layout and style preset stored separately from source-specific projects.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct LayoutPresetFile {
    pub version: u32,
    pub name: String,
    pub grid: GridSettings,
    pub tiles: Vec<LayoutPresetTile>,
    pub style: ProjectStyle,
    pub watermark: WatermarkSettings,
    pub export: ExportSettings,
}

impl LayoutPresetFile {
    /// Creates a preset from the reusable, non-source-specific parts of a project file.
    pub fn from_project(project: &ProjectFile, name: impl Into<String>) -> Self {
        Self {
            version: 1,
            name: name.into(),
            grid: project.grid.clone(),
            tiles: project
                .tiles
                .iter()
                .map(|tile| LayoutPresetTile {
                    id: tile.id.clone(),
                    order: tile.order,
                    span: tile.span.clone(),
                })
                .collect(),
            style: project.style.clone(),
            watermark: project.watermark.clone(),
            export: project.export.clone(),
        }
    }
}

/// Reusable tile layout entry stored inside a layout preset.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct LayoutPresetTile {
    pub id: String,
    pub order: u32,
    pub span: TileSpan,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
/// Decode depth selected for the current project.
pub enum AnalysisMode {
    QuickPreview,
    FullFidelity,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
/// Source media metadata stored in the project file.
pub struct VideoSource {
    pub path: String,
    pub duration_ms: Option<u64>,
    pub fps: Option<f64>,
    pub frame_count: Option<u64>,
    pub width: Option<u32>,
    pub height: Option<u32>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub crop: Option<VideoCrop>,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
/// Normalized crop rectangle relative to the decoded video frame.
pub struct VideoCrop {
    pub x: f64,
    pub y: f64,
    pub width: f64,
    pub height: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
/// Playback state that should survive save/load operations.
pub struct PlaybackSettings {
    pub playhead_ms: u64,
    pub active_frame_index: u64,
    pub custom_skip_ms: u64,
    pub frame_step: u32,
}

impl Default for PlaybackSettings {
    fn default() -> Self {
        Self {
            playhead_ms: 0,
            active_frame_index: 0,
            custom_skip_ms: 10_000,
            frame_step: 1,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
/// Inclusive working range used for auto-fill and export.
pub struct TimeRange {
    pub start_ms: u64,
    pub end_ms: u64,
    pub sample_start_ms: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
/// Global sheet grid configuration and sharpness defaults.
pub struct GridSettings {
    pub rows: u32,
    pub columns: u32,
    pub gutter_px: u32,
    pub outer_margin_px: u32,
    pub default_sharpness_window: u32,
}

impl Default for GridSettings {
    fn default() -> Self {
        Self {
            rows: 4,
            columns: 5,
            gutter_px: 12,
            outer_margin_px: 24,
            default_sharpness_window: 12,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
/// One positioned tile inside the grid layout.
pub struct ProjectTile {
    pub id: String,
    pub order: u32,
    pub span: TileSpan,
    pub selection: TileSelection,
    pub pinned: bool,
    pub fine_tune_offset_ms: i64,
}

impl ProjectTile {
    /// Creates a default unpinned tile for the given order index.
    pub fn auto(order: u32) -> Self {
        Self {
            id: format!("tile-{order}"),
            order,
            span: TileSpan::default(),
            selection: TileSelection::Auto,
            pinned: false,
            fine_tune_offset_ms: 0,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(tag = "kind", rename_all = "camelCase")]
/// Frame selection source for an individual tile.
pub enum TileSelection {
    #[serde(rename = "auto")]
    Auto,
    #[serde(rename = "manual")]
    ManualFrame {
        #[serde(rename = "frameIndex")]
        frame_index: u64,
        #[serde(rename = "timeMs")]
        time_ms: u64,
    },
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "camelCase")]
/// Grid occupancy for a tile, including row and column spans.
pub struct TileSpan {
    pub row: u32,
    pub column: u32,
    pub row_span: u32,
    pub column_span: u32,
}

impl Default for TileSpan {
    fn default() -> Self {
        Self {
            row: 0,
            column: 0,
            row_span: 1,
            column_span: 1,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
/// Sheet-wide styling applied during rendering.
pub struct ProjectStyle {
    pub frame_rounding_px: u32,
    pub frame_shadow_px: u32,
    pub frame_border_px: u32,
    pub show_metadata_bar: bool,
    pub show_timestamps: bool,
    pub dark_mode: bool,
}

impl Default for ProjectStyle {
    fn default() -> Self {
        Self {
            frame_rounding_px: 20,
            frame_shadow_px: 18,
            frame_border_px: 2,
            show_metadata_bar: true,
            show_timestamps: true,
            dark_mode: true,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
/// Optional watermark layers rendered on top of the sheet.
pub struct WatermarkSettings {
    pub text: Option<WatermarkText>,
    pub image: Option<WatermarkImage>,
}

impl Default for WatermarkSettings {
    fn default() -> Self {
        Self {
            text: Some(WatermarkText {
                value: "Preview".to_string(),
                opacity: 0.55,
            }),
            image: None,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
/// Text watermark configuration.
pub struct WatermarkText {
    pub value: String,
    pub opacity: f32,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
/// Image watermark configuration.
pub struct WatermarkImage {
    pub path: String,
    pub opacity: f32,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
/// Supported raster output formats.
pub enum ExportFormat {
    Png,
    Jpeg,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
/// Final export settings applied by the renderer and CLI.
pub struct ExportSettings {
    pub format: ExportFormat,
    pub scale: f32,
    pub output_path: Option<String>,
}

impl Default for ExportSettings {
    fn default() -> Self {
        Self {
            format: ExportFormat::Png,
            scale: 1.0,
            output_path: None,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
/// Batch-processing options reused across multiple source videos.
pub struct BatchSettings {
    pub enabled: bool,
    pub retain_manual_overrides: bool,
    pub inputs: Vec<String>,
}

#[cfg(test)]
mod tests {
    use super::{LayoutPresetFile, ProjectFile, TileSelection, VideoCrop, VideoSource};

    #[test]
    fn starter_project_places_tiles_across_the_default_grid() {
        let project = ProjectFile::starter("movie.mp4");

        assert_eq!(project.grid.rows, 4);
        assert_eq!(project.grid.columns, 5);
        assert_eq!(project.tiles.len(), 20);
        assert_eq!(project.tiles[0].span.row, 0);
        assert_eq!(project.tiles[0].span.column, 0);
        assert_eq!(project.tiles[5].span.row, 1);
        assert_eq!(project.tiles[5].span.column, 0);
        assert_eq!(project.tiles[19].span.row, 3);
        assert_eq!(project.tiles[19].span.column, 4);
    }

    #[test]
    fn tile_selection_serializes_with_frontend_shape() {
        let selection = TileSelection::ManualFrame {
            frame_index: 42,
            time_ms: 1_750,
        };

        let value = serde_json::to_value(selection).expect("selection should serialize");
        assert_eq!(
            value,
            serde_json::json!({
                "kind": "manual",
                "frameIndex": 42,
                "timeMs": 1_750,
            })
        );
    }

    #[test]
    fn video_source_serializes_optional_crop_with_frontend_shape() {
        let video = VideoSource {
            path: "movie.mp4".to_string(),
            duration_ms: Some(1_000),
            fps: Some(24.0),
            frame_count: Some(24),
            width: Some(1920),
            height: Some(1080),
            crop: Some(VideoCrop {
                x: 0.1,
                y: 0.2,
                width: 0.6,
                height: 0.5,
            }),
        };

        let value = serde_json::to_value(video).expect("video source should serialize");
        assert_eq!(
            value,
            serde_json::json!({
                "path": "movie.mp4",
                "durationMs": 1_000,
                "fps": 24.0,
                "frameCount": 24,
                "width": 1920,
                "height": 1080,
                "crop": {
                    "x": 0.1,
                    "y": 0.2,
                    "width": 0.6,
                    "height": 0.5
                }
            })
        );
    }

    #[test]
    fn layout_preset_serializes_without_source_specific_fields() {
        let project = ProjectFile::starter("movie.mp4");
        let preset = LayoutPresetFile::from_project(&project, "Default Grid");

        let value = serde_json::to_value(preset).expect("layout preset should serialize");
        assert_eq!(value["name"], "Default Grid");
        assert!(value.get("video").is_none());
        assert!(value.get("range").is_none());
        assert!(value.get("playback").is_none());
        assert!(value.get("batch").is_none());
        assert_eq!(value["tiles"].as_array().map(Vec::len), Some(20));
    }
}

impl Default for BatchSettings {
    fn default() -> Self {
        Self {
            enabled: false,
            retain_manual_overrides: false,
            inputs: Vec::new(),
        }
    }
}
