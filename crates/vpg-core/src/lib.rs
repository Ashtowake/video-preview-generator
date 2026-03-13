//! Core domain types and helpers for the Video Preview Generator rewrite.

pub mod diagnostics;
pub mod grid;
pub mod media;
pub mod project;
pub mod render;
pub mod seek;
pub mod services;
pub mod validation;

pub use diagnostics::{DiagnosticsBundle, NoticeItem};
pub use grid::{assign_auto_tiles, validate_grid_spans, GridValidationIssue};
pub use media::{PreviewFrame, VideoProbe};
pub use project::{
    AnalysisMode, BatchSettings, ExportFormat, ExportSettings, GridSettings, LayoutPresetFile,
    LayoutPresetTile, PlaybackSettings, ProjectFile, ProjectStyle, ProjectTile, TileSelection,
    TileSpan, TimeRange, VideoCrop, VideoSource, WatermarkImage, WatermarkSettings,
    WatermarkText,
};
pub use render::{ExportResult, SheetPreview};
pub use seek::{
    center_of_bin_samples, clamp_frame_index, clamp_playhead_ms, display_frame_number,
    evenly_spaced_samples_from_start, parse_time_delta, step_by_frames, step_by_time,
};
pub use services::{DecodeEstimate, MediaService, RenderPlan, ServiceStatus};
pub use validation::{validate_project, ValidationIssue, ValidationLevel};
