//! Core domain types and helpers for the Video Preview Generator rewrite.

pub mod diagnostics;
pub mod grid;
pub mod project;
pub mod seek;
pub mod services;
pub mod validation;

pub use diagnostics::{DiagnosticsBundle, NoticeItem};
pub use grid::{assign_auto_tiles, validate_grid_spans, GridValidationIssue};
pub use project::{
    AnalysisMode, BatchSettings, ExportFormat, ExportSettings, GridSettings, PlaybackSettings,
    ProjectFile, ProjectStyle, ProjectTile, TileSelection, TileSpan, TimeRange, VideoSource,
    WatermarkImage, WatermarkSettings, WatermarkText,
};
pub use seek::{
    center_of_bin_samples, clamp_playhead_ms, parse_time_delta, step_by_frames, step_by_time,
};
pub use services::{DecodeEstimate, RenderPlan, ServiceStatus, StubMediaService};
pub use validation::{validate_project, ValidationIssue, ValidationLevel};
