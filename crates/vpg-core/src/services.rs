use serde::{Deserialize, Serialize};

use crate::project::ProjectFile;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct DecodeEstimate {
    pub projected_memory_mb: u64,
    pub projected_cache_mb: u64,
    pub can_upgrade_to_full_fidelity: bool,
    pub reason: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
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

/// Temporary service facade until FFmpeg-backed implementations land.
#[derive(Debug, Default)]
pub struct StubMediaService;

impl StubMediaService {
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

    pub fn render_plan(&self, project: &ProjectFile) -> RenderPlan {
        RenderPlan {
            status: ServiceStatus::Placeholder,
            message: format!(
                "Render pipeline not wired yet for {} tiles and {}x{} grid",
                project.tiles.len(),
                project.grid.rows,
                project.grid.columns
            ),
        }
    }
}
