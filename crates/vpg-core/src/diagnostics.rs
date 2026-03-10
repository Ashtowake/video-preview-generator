use serde::{Deserialize, Serialize};

use crate::project::ProjectFile;

/// A lightweight manifest that can later be written alongside logs and cache metadata.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct DiagnosticsBundle {
    pub app_version: String,
    pub analysis_mode: String,
    pub video_path: String,
    pub cache_directory: String,
    pub notices: Vec<NoticeItem>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct NoticeItem {
    pub name: String,
    pub license: String,
    pub url: String,
}

impl DiagnosticsBundle {
    pub fn from_project(project: &ProjectFile, cache_directory: impl Into<String>) -> Self {
        Self {
            app_version: env!("CARGO_PKG_VERSION").to_string(),
            analysis_mode: format!("{:?}", project.analysis_mode),
            video_path: project.video.path.clone(),
            cache_directory: cache_directory.into(),
            notices: vec![
                NoticeItem {
                    name: "FFmpeg".to_string(),
                    license: "LGPL/GPL depending on distribution".to_string(),
                    url: "https://ffmpeg.org/legal.html".to_string(),
                },
                NoticeItem {
                    name: "Tauri".to_string(),
                    license: "MIT OR Apache-2.0".to_string(),
                    url: "https://v2.tauri.app".to_string(),
                },
            ],
        }
    }
}
