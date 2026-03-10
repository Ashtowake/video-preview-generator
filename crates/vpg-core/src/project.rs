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

        Self {
            version: 1,
            analysis_mode: AnalysisMode::QuickPreview,
            video: VideoSource {
                path: video_path.into(),
                duration_ms: Some(60_000),
                fps: Some(24.0),
                width: Some(1920),
                height: Some(1080),
            },
            playback: PlaybackSettings::default(),
            range: TimeRange {
                start_ms: 0,
                end_ms: 60_000,
            },
            grid,
            tiles: (0..total_tiles)
                .map(|index| ProjectTile::auto(index as u32))
                .collect(),
            style: ProjectStyle::default(),
            watermark: WatermarkSettings::default(),
            export: ExportSettings::default(),
            batch: BatchSettings::default(),
        }
    }
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum AnalysisMode {
    QuickPreview,
    FullFidelity,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct VideoSource {
    pub path: String,
    pub duration_ms: Option<u64>,
    pub fps: Option<f64>,
    pub width: Option<u32>,
    pub height: Option<u32>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
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
pub struct TimeRange {
    pub start_ms: u64,
    pub end_ms: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
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
pub struct ProjectTile {
    pub id: String,
    pub order: u32,
    pub span: TileSpan,
    pub selection: TileSelection,
    pub pinned: bool,
    pub fine_tune_offset_ms: i64,
}

impl ProjectTile {
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
#[serde(rename_all = "camelCase")]
pub enum TileSelection {
    Auto,
    ManualFrame { frame_index: u64, time_ms: u64 },
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "camelCase")]
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
pub struct WatermarkText {
    pub value: String,
    pub opacity: f32,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct WatermarkImage {
    pub path: String,
    pub opacity: f32,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum ExportFormat {
    Png,
    Jpeg,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
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
pub struct BatchSettings {
    pub enabled: bool,
    pub retain_manual_overrides: bool,
    pub inputs: Vec<String>,
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
