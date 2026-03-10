mod preview_cache;

use std::fs;
use std::sync::Mutex;

use tauri::Manager;
use vpg_core::{
    DiagnosticsBundle, ExportResult, MediaService, PreviewFrame, ProjectFile, RenderPlan,
};

use crate::preview_cache::{PreviewCache, PreviewCacheKey};

#[derive(Default)]
struct AppState {
    preview_cache: Mutex<PreviewCache>,
}

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
fn probe_video(video_path: String) -> Result<ProjectFile, String> {
    MediaService
        .load_video_project(&video_path)
        .map_err(|error| error.to_string())
}

#[tauri::command]
fn estimate_full_fidelity(project: ProjectFile) -> vpg_core::DecodeEstimate {
    MediaService.estimate_full_fidelity(&project)
}

#[tauri::command]
fn preview_frame(
    state: tauri::State<'_, AppState>,
    project: ProjectFile,
    time_ms: u64,
    max_width: Option<u32>,
) -> Result<PreviewFrame, String> {
    let max_width = max_width.unwrap_or(640);
    let cache_key = PreviewCacheKey::from_project(&project, time_ms, max_width);

    if let Some(frame) = state
        .preview_cache
        .lock()
        .map_err(|_| "preview cache lock poisoned".to_string())?
        .get(&cache_key)
    {
        return Ok(frame);
    }

    let frame = MediaService
        .preview_frame(&project, time_ms, max_width)
        .map_err(|error| error.to_string())?;

    state
        .preview_cache
        .lock()
        .map_err(|_| "preview cache lock poisoned".to_string())?
        .insert(cache_key, frame.clone());

    Ok(frame)
}

#[tauri::command]
fn find_sharpest_neighbours(
    project: ProjectFile,
    tile_ids: Vec<String>,
) -> Result<ProjectFile, String> {
    MediaService
        .find_sharpest_neighbours(&project, &tile_ids)
        .map_err(|error| error.to_string())
}

#[tauri::command]
fn diagnostics_bundle(project: ProjectFile) -> DiagnosticsBundle {
    DiagnosticsBundle::from_project(&project, "./cache")
}

#[tauri::command]
fn render_plan(project: ProjectFile) -> RenderPlan {
    MediaService.render_plan(&project)
}

#[tauri::command]
fn export_project(
    project: ProjectFile,
    output_path: Option<String>,
) -> Result<ExportResult, String> {
    MediaService
        .export_sheet(&project, output_path.as_deref())
        .map_err(|error| error.to_string())
}

#[tauri::command]
fn release_notes() -> Vec<String> {
    vec![
        "Workspace rewrite scaffold with Rust core, CLI, and desktop shell.".to_string(),
        "Media/range pane now includes transport controls and import mode surfaces.".to_string(),
        "Stage 2 adds real ffprobe/ffmpeg probing, sharpness search, and sheet export.".to_string(),
    ]
}

pub fn run() {
    tauri::Builder::default()
        .manage(AppState::default())
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
            probe_video,
            save_project,
            load_project,
            estimate_full_fidelity,
            preview_frame,
            find_sharpest_neighbours,
            diagnostics_bundle,
            render_plan,
            export_project,
            release_notes
        ])
        .run(tauri::generate_context!())
        .expect("failed to run Video Preview Generator");
}
