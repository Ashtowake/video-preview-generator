use std::fs;
use std::path::{Path, PathBuf};
use std::sync::Mutex;
use std::thread;
use std::time::{Duration, Instant};

use anyhow::{anyhow, Context, Result};
use base64::engine::general_purpose::STANDARD as BASE64_STANDARD;
use base64::Engine;
use serde_json::{json, Value};
use tauri::{AppHandle, Manager, Runtime};
use tauri_plugin_mpv::{MpvCommand, MpvConfig, MpvExt};
use vpg_core::seek::{clamp_frame_index, frame_index_at_time_ms};
use vpg_core::{PreviewFrame, ProjectFile};

const MPV_WINDOW_LABEL: &str = "main";
const MPV_COMMAND_TIMEOUT_MS: u64 = 2_000;
const MPV_SETTLE_TIMEOUT_MS: u64 = 300;
const MPV_SETTLE_POLL_MS: u64 = 10;

/// Mutable state for the shared headless mpv preview session.
///
/// The session is serialized behind a mutex so seek and screenshot commands never race each other.
#[derive(Debug, Default)]
pub struct MpvTransportState {
    loaded_video_path: Option<String>,
    screenshot_serial: u64,
}

/// Seeks the persistent mpv session to the requested time and captures a PNG preview.
pub fn preview_frame_from_transport<R: Runtime>(
    app: &AppHandle<R>,
    transport_state: &Mutex<MpvTransportState>,
    project: &ProjectFile,
    time_ms: u64,
) -> Result<PreviewFrame> {
    let mut session = transport_state
        .lock()
        .map_err(|_| anyhow!("mpv transport lock poisoned"))?;

    ensure_session(app)?;
    ensure_video_loaded(app, &mut session, &project.video.path)?;

    let safe_time_ms = clamp_seek_time_ms(project, time_ms);
    seek_exact(app, safe_time_ms)?;
    capture_current_frame(app, &mut session, project)
}

fn ensure_session<R: Runtime>(app: &AppHandle<R>) -> Result<()> {
    let config = MpvConfig {
        path: "mpv".to_string(),
        args: vec![
            "--vo=null".to_string(),
            "--ao=null".to_string(),
            "--idle=yes".to_string(),
            "--keep-open=yes".to_string(),
            "--pause=yes".to_string(),
            "--mute=yes".to_string(),
            "--hr-seek=yes".to_string(),
            "--cache=yes".to_string(),
            "--demuxer-thread=yes".to_string(),
            "--input-default-bindings=no".to_string(),
            "--input-vo-keyboard=no".to_string(),
        ],
        observed_properties: Vec::new(),
        ipc_timeout_ms: MPV_COMMAND_TIMEOUT_MS,
        show_mpv_output: false,
    };

    app.mpv()
        .init(config, MPV_WINDOW_LABEL)
        .context("failed to initialize persistent mpv preview session")?;

    Ok(())
}

fn ensure_video_loaded<R: Runtime>(
    app: &AppHandle<R>,
    session: &mut MpvTransportState,
    video_path: &str,
) -> Result<()> {
    let source_path = Path::new(video_path);
    if !source_path.is_file() {
        return Err(anyhow!(
            "source video does not exist for mpv preview: {}",
            source_path.display()
        ));
    }

    let current_path = get_property_string(app, "path").unwrap_or(None);
    if session.loaded_video_path.as_deref() == Some(video_path)
        && current_path.as_deref() == Some(video_path)
    {
        return Ok(());
    }

    run_command(
        app,
        vec![json!("loadfile"), json!(video_path), json!("replace")],
    )?;
    run_command(app, vec![json!("set_property"), json!("pause"), json!(true)])?;
    wait_for_settle(app)?;

    session.loaded_video_path = Some(video_path.to_string());
    Ok(())
}

fn seek_exact<R: Runtime>(app: &AppHandle<R>, time_ms: u64) -> Result<()> {
    let seconds = time_ms as f64 / 1000.0;
    run_command(
        app,
        vec![json!("seek"), json!(seconds), json!("absolute+exact")],
    )?;
    run_command(app, vec![json!("set_property"), json!("pause"), json!(true)])?;
    wait_for_settle(app)?;
    Ok(())
}

fn capture_current_frame<R: Runtime>(
    app: &AppHandle<R>,
    session: &mut MpvTransportState,
    project: &ProjectFile,
) -> Result<PreviewFrame> {
    let preview_dir = app
        .path()
        .app_cache_dir()
        .context("failed to resolve app cache directory")?
        .join("mpv-previews");
    fs::create_dir_all(&preview_dir).with_context(|| {
        format!(
            "failed to create mpv preview directory {}",
            preview_dir.display()
        )
    })?;

    session.screenshot_serial = session.screenshot_serial.wrapping_add(1);
    let screenshot_path = preview_dir.join(format!("preview-{:016x}.png", session.screenshot_serial));

    run_command(
        app,
        vec![
            json!("screenshot-to-file"),
            json!(screenshot_path.to_string_lossy().to_string()),
            json!("video"),
        ],
    )?;

    let bytes = wait_for_screenshot_bytes(&screenshot_path)?;
    let _ = fs::remove_file(&screenshot_path);

    let actual_time_ms = current_time_ms(app)?;
    let fps = project.video.fps.unwrap_or(24.0);

    Ok(PreviewFrame {
        data_url: format!("data:image/png;base64,{}", BASE64_STANDARD.encode(bytes)),
        time_ms: actual_time_ms,
        frame_index: clamp_frame_index(
            frame_index_at_time_ms(actual_time_ms, fps),
            project.video.frame_count,
        ),
    })
}

fn wait_for_screenshot_bytes(path: &PathBuf) -> Result<Vec<u8>> {
    let deadline = Instant::now() + Duration::from_millis(MPV_SETTLE_TIMEOUT_MS);

    loop {
        if let Ok(bytes) = fs::read(path) {
            return Ok(bytes);
        }

        if Instant::now() >= deadline {
            return Err(anyhow!(
                "mpv did not finish writing preview screenshot {} in time",
                path.display()
            ));
        }

        thread::sleep(Duration::from_millis(MPV_SETTLE_POLL_MS));
    }
}

fn wait_for_settle<R: Runtime>(app: &AppHandle<R>) -> Result<()> {
    let deadline = Instant::now() + Duration::from_millis(MPV_SETTLE_TIMEOUT_MS);

    loop {
        if !get_property_bool(app, "seeking")?.unwrap_or(false) {
            return Ok(());
        }

        if Instant::now() >= deadline {
            return Ok(());
        }

        thread::sleep(Duration::from_millis(MPV_SETTLE_POLL_MS));
    }
}

fn current_time_ms<R: Runtime>(app: &AppHandle<R>) -> Result<u64> {
    let seconds = get_property_f64(app, "time-pos")?.unwrap_or(0.0);
    Ok((seconds * 1000.0).round().max(0.0) as u64)
}

fn get_property_bool<R: Runtime>(app: &AppHandle<R>, property: &str) -> Result<Option<bool>> {
    Ok(run_command(app, vec![json!("get_property"), json!(property)])?
        .and_then(|value| value.as_bool()))
}

fn get_property_f64<R: Runtime>(app: &AppHandle<R>, property: &str) -> Result<Option<f64>> {
    Ok(run_command(app, vec![json!("get_property"), json!(property)])?
        .and_then(|value| value.as_f64()))
}

fn get_property_string<R: Runtime>(app: &AppHandle<R>, property: &str) -> Result<Option<String>> {
    Ok(run_command(app, vec![json!("get_property"), json!(property)])?
        .and_then(|value| value.as_str().map(ToOwned::to_owned)))
}

fn run_command<R: Runtime>(app: &AppHandle<R>, command: Vec<Value>) -> Result<Option<Value>> {
    let response = app
        .mpv()
        .command(
            MpvCommand {
                command,
                request_id: None,
            },
            MPV_WINDOW_LABEL,
        )
        .context("failed to send command to persistent mpv preview session")?;

    if response.error != "success" {
        return Err(anyhow!("mpv command failed: {}", response.error));
    }

    Ok(response.data)
}

fn clamp_seek_time_ms(project: &ProjectFile, time_ms: u64) -> u64 {
    let duration_ms = project.video.duration_ms.unwrap_or(60_000);
    let latest_seek_ms = duration_ms.saturating_sub(1);
    time_ms.min(latest_seek_ms)
}
