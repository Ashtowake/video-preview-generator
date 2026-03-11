use std::path::Path;

use anyhow::Result;
use serde::{Deserialize, Serialize};

use crate::media::{create_project_from_probe, find_sharpest_neighbours, preview_frame};
use crate::project::ProjectFile;
use crate::render::{export_sheet, render_preview, ExportResult, SheetPreview};

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
/// Coarse safety estimate used before enabling full-fidelity mode.
pub struct DecodeEstimate {
    pub projected_memory_mb: u64,
    pub projected_cache_mb: u64,
    pub can_upgrade_to_full_fidelity: bool,
    pub reason: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
/// High-level renderer status exposed to the UI and CLI.
pub struct RenderPlan {
    pub status: ServiceStatus,
    pub message: String,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum ServiceStatus {
    Placeholder,
    Ready,
}

#[derive(Debug, Default)]
/// Thin façade over the Rust media pipeline used by Tauri commands and the CLI.
pub struct MediaService;

impl MediaService {
    /// Estimates whether the current project can safely switch to full-fidelity controls.
    pub fn estimate_full_fidelity(&self, project: &ProjectFile) -> DecodeEstimate {
        let duration_s = project.video.duration_ms.unwrap_or(60_000) / 1000;
        let width = project.video.width.unwrap_or(1920) as u64;
        let height = project.video.height.unwrap_or(1080) as u64;
        let fps = project.video.fps.unwrap_or(24.0);
        let projected_memory_mb = (width * height * 4 / 1_048_576).max(32);
        let projected_cache_mb = ((duration_s as f64 * fps / 8.0).round() as u64).max(64);
        let can_upgrade = projected_cache_mb < 2_048;

        DecodeEstimate {
            projected_memory_mb,
            projected_cache_mb,
            can_upgrade_to_full_fidelity: can_upgrade,
            reason: (!can_upgrade).then_some(
                "estimated cache size exceeds the current first-pass safety limit".to_string(),
            ),
        }
    }

    /// Returns a short human-readable summary of the current render readiness.
    pub fn render_plan(&self, project: &ProjectFile) -> RenderPlan {
        RenderPlan {
            status: ServiceStatus::Ready,
            message: format!(
                "ready to render {} tiles from {}",
                project.tiles.len(),
                Path::new(&project.video.path)
                    .file_name()
                    .and_then(|value| value.to_str())
                    .unwrap_or(&project.video.path)
            ),
        }
    }

    /// Probes a video file and returns a populated starter project.
    pub fn load_video_project(&self, video_path: &str) -> Result<ProjectFile> {
        create_project_from_probe(video_path)
    }

    /// Returns a preview frame for the requested project time.
    pub fn preview_frame(
        &self,
        project: &ProjectFile,
        time_ms: u64,
        max_width: u32,
    ) -> Result<crate::media::PreviewFrame> {
        preview_frame(project, time_ms, max_width)
    }

    /// Replaces the selected tiles with locally sharper neighbours.
    pub fn find_sharpest_neighbours(
        &self,
        project: &ProjectFile,
        tile_ids: &[String],
    ) -> Result<ProjectFile> {
        find_sharpest_neighbours(project, tile_ids, project.grid.default_sharpness_window)
    }

    /// Renders and writes the final sheet image.
    pub fn export_sheet(
        &self,
        project: &ProjectFile,
        output_override: Option<&str>,
    ) -> Result<ExportResult> {
        export_sheet(project, output_override)
    }

    /// Renders an editor-sized preview of the current contact sheet.
    pub fn render_preview(
        &self,
        project: &ProjectFile,
        max_width: Option<u32>,
    ) -> Result<SheetPreview> {
        render_preview(project, max_width)
    }
}
