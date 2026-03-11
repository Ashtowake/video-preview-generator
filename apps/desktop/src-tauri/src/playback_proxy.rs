use std::collections::hash_map::DefaultHasher;
use std::fs;
use std::hash::{Hash, Hasher};
use std::path::{Path, PathBuf};
use std::process::Command;
use std::time::UNIX_EPOCH;

use anyhow::{anyhow, Context, Result};
use base64::engine::general_purpose::STANDARD as BASE64_STANDARD;
use base64::Engine;
use serde::{Deserialize, Serialize};
use tauri::{AppHandle, Manager, Runtime};

const PROXY_MAX_WIDTH: u32 = 854;

/// Preview media payload returned to the frontend for embedded playback.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct PreparedPlayback {
    pub mime_type: String,
    pub base64_data: String,
}

/// Resolves a cached playback proxy for the given source video, creating it on first use.
pub fn ensure_playback_proxy<R: Runtime>(app: &AppHandle<R>, video_path: &str) -> Result<PathBuf> {
    let source_path = Path::new(video_path);
    if !source_path.is_file() {
        return Err(anyhow!(
            "source video does not exist: {}",
            source_path.display()
        ));
    }

    let proxy_dir = app
        .path()
        .app_cache_dir()
        .context("failed to resolve app cache directory")?
        .join("playback-proxies");
    fs::create_dir_all(&proxy_dir).with_context(|| {
        format!(
            "failed to create proxy cache directory {}",
            proxy_dir.display()
        )
    })?;

    let proxy_path = proxy_dir.join(proxy_file_name(source_path));
    if proxy_path.is_file() {
        allow_asset_file(app, &proxy_path)?;
        return Ok(proxy_path);
    }

    let status = Command::new("ffmpeg")
        .arg("-y")
        .arg("-loglevel")
        .arg("error")
        .arg("-nostdin")
        .arg("-i")
        .arg(source_path)
        .arg("-an")
        .arg("-vf")
        .arg(format!(
            "scale='min(iw,{PROXY_MAX_WIDTH})':-2:flags=lanczos"
        ))
        .args(proxy_codec_args())
        .arg(&proxy_path)
        .status()
        .with_context(|| {
            format!(
                "failed to create playback proxy for {}",
                source_path.display()
            )
        })?;

    if !status.success() {
        return Err(anyhow!(
            "ffmpeg failed while creating playback proxy for {}",
            source_path.display()
        ));
    }

    allow_asset_file(app, &proxy_path)?;
    Ok(proxy_path)
}

/// Loads preview media as a frontend-consumable blob payload.
pub fn prepare_playback_payload<R: Runtime>(
    app: &AppHandle<R>,
    video_path: &str,
) -> Result<PreparedPlayback> {
    let proxy_path = ensure_playback_proxy(app, video_path)?;
    let bytes = fs::read(&proxy_path).with_context(|| {
        format!(
            "failed to read playback proxy payload {}",
            proxy_path.display()
        )
    })?;

    Ok(PreparedPlayback {
        mime_type: proxy_mime_type().to_string(),
        base64_data: BASE64_STANDARD.encode(bytes),
    })
}

fn allow_asset_file<R: Runtime>(app: &AppHandle<R>, path: &Path) -> Result<()> {
    app.asset_protocol_scope()
        .allow_file(path)
        .with_context(|| format!("failed to allow asset file {}", path.display()))?;

    Ok(())
}

fn proxy_file_name(source_path: &Path) -> String {
    let mut hasher = DefaultHasher::new();
    source_path.to_string_lossy().hash(&mut hasher);
    let metadata = fs::metadata(source_path).ok();
    metadata.as_ref().map(|entry| entry.len()).hash(&mut hasher);
    metadata
        .as_ref()
        .and_then(|entry| entry.modified().ok())
        .and_then(|modified| modified.duration_since(UNIX_EPOCH).ok())
        .map(|duration| duration.as_millis())
        .hash(&mut hasher);

    format!("{:016x}.{}", hasher.finish(), proxy_extension())
}

#[cfg(target_os = "linux")]
fn proxy_extension() -> &'static str {
    "webm"
}

#[cfg(not(target_os = "linux"))]
fn proxy_extension() -> &'static str {
    "mp4"
}

#[cfg(target_os = "linux")]
fn proxy_mime_type() -> &'static str {
    "video/webm"
}

#[cfg(not(target_os = "linux"))]
fn proxy_mime_type() -> &'static str {
    "video/mp4"
}

#[cfg(target_os = "linux")]
fn proxy_codec_args() -> &'static [&'static str] {
    &[
        "-c:v",
        "libvpx",
        "-deadline",
        "realtime",
        "-cpu-used",
        "8",
        "-crf",
        "36",
        "-b:v",
        "0",
        "-pix_fmt",
        "yuv420p",
    ]
}

#[cfg(not(target_os = "linux"))]
fn proxy_codec_args() -> &'static [&'static str] {
    &[
        "-c:v",
        "libx264",
        "-preset",
        "ultrafast",
        "-crf",
        "32",
        "-pix_fmt",
        "yuv420p",
        "-movflags",
        "+faststart",
    ]
}
