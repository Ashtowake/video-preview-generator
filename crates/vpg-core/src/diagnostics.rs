use serde::{Deserialize, Serialize};

use crate::project::{AnalysisMode, ProjectFile};

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
            analysis_mode: analysis_mode_label(project.analysis_mode),
            video_path: project.video.path.clone(),
            cache_directory: cache_directory.into(),
            notices: vec![
                NoticeItem {
                    name: "libmpv / mpv".to_string(),
                    license:
                        "GPL-2.0-or-later or LGPL-2.1-or-later depending on build configuration"
                            .to_string(),
                    url: "https://github.com/mpv-player/mpv".to_string(),
                },
                NoticeItem {
                    name: "Qt 6".to_string(),
                    license:
                        "LGPL-3.0-only, GPL-2.0-only, GPL-3.0-only, or commercial depending on module and distribution"
                            .to_string(),
                    url: "https://doc.qt.io/qt-6/licensing.html".to_string(),
                },
                NoticeItem {
                    name: "FFmpeg".to_string(),
                    license:
                        "LGPL-2.1-or-later by default, or GPL when built with GPL components enabled"
                            .to_string(),
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

fn analysis_mode_label(mode: AnalysisMode) -> String {
    match mode {
        AnalysisMode::QuickPreview => "quick_preview".to_string(),
        AnalysisMode::FullFidelity => "full_fidelity".to_string(),
    }
}
