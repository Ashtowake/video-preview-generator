use std::fs;

use tauri::Manager;
use vpg_core::{DiagnosticsBundle, ProjectFile, StubMediaService};

#[tauri::command]
fn starter_project(video_path: Option<String>) -> ProjectFile {
    ProjectFile::starter(video_path.unwrap_or_else(|| "unloaded-video.mp4".to_string()))
}

#[tauri::command]
fn save_project(path: String, project: ProjectFile) -> Result<(), String> {
    let json = serde_json::to_string_pretty(&project).map_err(|error| error.to_string())?;
    fs::write(path, json).map_err(|error| error.to_string())
}

#[tauri::command]
fn load_project(path: String) -> Result<ProjectFile, String> {
    let contents = fs::read_to_string(path).map_err(|error| error.to_string())?;
    serde_json::from_str(&contents).map_err(|error| error.to_string())
}

#[tauri::command]
fn estimate_full_fidelity(project: ProjectFile) -> vpg_core::DecodeEstimate {
    StubMediaService.estimate_full_fidelity(&project)
}

#[tauri::command]
fn diagnostics_bundle(project: ProjectFile) -> DiagnosticsBundle {
    DiagnosticsBundle::from_project(&project, "./cache")
}

#[tauri::command]
fn render_plan(project: ProjectFile) -> vpg_core::RenderPlan {
    StubMediaService.render_plan(&project)
}

#[tauri::command]
fn release_notes() -> Vec<String> {
    vec![
        "Workspace rewrite scaffold with Rust core, CLI, and desktop shell.".to_string(),
        "Media/range pane now includes transport controls and import mode surfaces.".to_string(),
        "FFmpeg-backed decode, sharpness analysis, and final export remain the next milestone."
            .to_string(),
    ]
}

pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_fs::init())
        .plugin(tauri_plugin_log::Builder::default().build())
        .plugin(tauri_plugin_shell::init())
        .plugin(tauri_plugin_updater::Builder::new().build())
        .setup(|app| {
            if let Some(window) = app.get_webview_window("main") {
                let _ = window.set_title("Video Preview Generator");
            }
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            starter_project,
            save_project,
            load_project,
            estimate_full_fidelity,
            diagnostics_bundle,
            render_plan,
            release_notes
        ])
        .run(tauri::generate_context!())
        .expect("failed to run Video Preview Generator");
}
