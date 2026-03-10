//! FFmpeg-backed media probing, preview extraction, and local sharpness analysis.

use std::io::Cursor;
use std::path::Path;
use std::process::Command;

use anyhow::{anyhow, Context, Result};
use base64::engine::general_purpose::STANDARD as BASE64_STANDARD;
use base64::Engine;
use image::{DynamicImage, ImageFormat};
use serde::{Deserialize, Serialize};

use crate::grid::assign_auto_tiles;
use crate::project::{PlaybackSettings, ProjectFile, TileSelection, TimeRange};

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
/// Media metadata returned from `ffprobe` and mirrored to the frontend.
pub struct VideoProbe {
    pub duration_ms: u64,
    pub fps: f64,
    pub frame_count: u64,
    pub width: u32,
    pub height: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
/// A PNG-encoded preview frame that can be shown directly in the desktop UI.
pub struct PreviewFrame {
    pub data_url: String,
    pub time_ms: u64,
    pub frame_index: u64,
}

#[derive(Debug, Deserialize)]
struct FfprobeOutput {
    #[serde(default)]
    streams: Vec<FfprobeStream>,
    #[serde(default)]
    format: Option<FfprobeFormat>,
}

#[derive(Debug, Deserialize)]
struct FfprobeStream {
    width: Option<u32>,
    height: Option<u32>,
    avg_frame_rate: Option<String>,
    r_frame_rate: Option<String>,
    nb_frames: Option<String>,
    duration: Option<String>,
}

#[derive(Debug, Deserialize)]
struct FfprobeFormat {
    duration: Option<String>,
}

/// Builds a starter project by probing the source video and auto-filling the initial grid.
pub fn create_project_from_probe(video_path: &str) -> Result<ProjectFile> {
    let path = Path::new(video_path);
    let probe = probe_video(path)?;
    let mut project = ProjectFile::starter(video_path.to_string());
    project.video.duration_ms = Some(probe.duration_ms);
    project.video.fps = Some(probe.fps);
    project.video.frame_count = Some(probe.frame_count);
    project.video.width = Some(probe.width);
    project.video.height = Some(probe.height);
    project.range = TimeRange {
        start_ms: 0,
        end_ms: probe.duration_ms.max(1_000),
    };
    project.playback = PlaybackSettings {
        playhead_ms: 0,
        active_frame_index: 0,
        custom_skip_ms: 10_000,
        frame_step: 1,
    };
    assign_auto_tiles(&mut project.tiles, 0, project.range.end_ms, probe.fps);
    Ok(project)
}

/// Reads core stream metadata with `ffprobe`.
///
/// The result intentionally falls back to conservative defaults when the container omits
/// optional values such as `nb_frames` or stream duration.
pub fn probe_video(video_path: &Path) -> Result<VideoProbe> {
    let output = Command::new(ffprobe_binary())
        .arg("-v")
        .arg("error")
        .arg("-select_streams")
        .arg("v:0")
        .arg("-show_entries")
        .arg("stream=width,height,avg_frame_rate,r_frame_rate,nb_frames,duration")
        .arg("-show_entries")
        .arg("format=duration")
        .arg("-of")
        .arg("json")
        .arg(video_path)
        .output()
        .with_context(|| format!("failed to run ffprobe on {}", video_path.display()))?;

    if !output.status.success() {
        return Err(anyhow!(
            "ffprobe failed: {}",
            String::from_utf8_lossy(&output.stderr)
        ));
    }

    let parsed: FfprobeOutput =
        serde_json::from_slice(&output.stdout).context("failed to parse ffprobe JSON output")?;
    let stream = parsed
        .streams
        .first()
        .ok_or_else(|| anyhow!("no video stream found"))?;

    let fps = parse_frame_rate(
        stream
            .avg_frame_rate
            .as_deref()
            .or(stream.r_frame_rate.as_deref())
            .unwrap_or("0/1"),
    )
    .filter(|fps| *fps > 0.0)
    .unwrap_or(24.0);
    let duration_seconds = stream
        .duration
        .as_deref()
        .or(parsed
            .format
            .as_ref()
            .and_then(|format| format.duration.as_deref()))
        .and_then(|value| value.parse::<f64>().ok())
        .unwrap_or(60.0);
    let duration_ms = (duration_seconds * 1000.0).round() as u64;
    let frame_count = stream
        .nb_frames
        .as_deref()
        .and_then(|value| value.parse::<u64>().ok())
        .unwrap_or_else(|| ((duration_ms as f64 / 1000.0) * fps).round() as u64);

    Ok(VideoProbe {
        duration_ms,
        fps,
        frame_count,
        width: stream.width.unwrap_or(1920),
        height: stream.height.unwrap_or(1080),
    })
}

/// Extracts a preview frame and returns it as an inline PNG data URL.
pub fn preview_frame(project: &ProjectFile, time_ms: u64, max_width: u32) -> Result<PreviewFrame> {
    let image = extract_frame_image(Path::new(&project.video.path), time_ms, Some(max_width))?;
    let mut bytes = Vec::new();
    image
        .write_to(&mut Cursor::new(&mut bytes), ImageFormat::Png)
        .context("failed to encode preview frame as PNG")?;
    let frame_index =
        ((time_ms as f64 / 1000.0) * project.video.fps.unwrap_or(24.0)).round() as u64;

    Ok(PreviewFrame {
        data_url: format!("data:image/png;base64,{}", BASE64_STANDARD.encode(bytes)),
        time_ms,
        frame_index,
    })
}

/// Replaces the selected manual tiles with the sharpest frame found in a local frame window.
///
/// Candidates are scored with variance of the Laplacian on a downscaled grayscale image.
/// When two candidates score the same, the closer frame wins so the operation stays stable.
pub fn find_sharpest_neighbours(
    project: &ProjectFile,
    tile_ids: &[String],
    window_size: u32,
) -> Result<ProjectFile> {
    let fps = project.video.fps.unwrap_or(24.0);
    let path = Path::new(&project.video.path);
    let mut next_project = project.clone();

    for tile in &mut next_project.tiles {
        if !tile_ids.iter().any(|tile_id| tile_id == &tile.id) {
            continue;
        }

        let (current_frame, current_time) = match tile.selection {
            TileSelection::ManualFrame {
                frame_index,
                time_ms,
            } => (frame_index, time_ms),
            TileSelection::Auto => continue,
        };

        let mut best_candidate = (current_frame, current_time, f64::MIN, 0_i64);
        for offset in -(window_size as i64)..=(window_size as i64) {
            let candidate_frame = current_frame as i64 + offset;
            if candidate_frame < 0 {
                continue;
            }

            let candidate_time = ((candidate_frame as f64 / fps) * 1000.0).round().max(0.0) as u64;
            let image = extract_frame_image(path, candidate_time, Some(320))?;
            let score = laplacian_variance(&image);
            let better_score = score > best_candidate.2;
            // Tie-break toward the nearest frame so repeated runs do not drift unnecessarily.
            let same_score_closer = (score - best_candidate.2).abs() < f64::EPSILON
                && offset.abs() < best_candidate.3.abs();

            if better_score || same_score_closer {
                best_candidate = (candidate_frame as u64, candidate_time, score, offset);
            }
        }

        tile.selection = TileSelection::ManualFrame {
            frame_index: best_candidate.0,
            time_ms: best_candidate.1,
        };
    }

    Ok(next_project)
}

/// Resolves the effective presentation timestamp for a tile after fine-tune offsets.
pub fn effective_tile_time_ms(
    project: &ProjectFile,
    selection: &TileSelection,
    fine_tune_offset_ms: i64,
) -> u64 {
    let duration_ms = project.video.duration_ms.unwrap_or(60_000) as i64;
    let base_time = match selection {
        TileSelection::Auto => 0,
        TileSelection::ManualFrame { time_ms, .. } => *time_ms as i64,
    };

    (base_time + fine_tune_offset_ms).clamp(0, duration_ms) as u64
}

/// Extracts a single frame with `ffmpeg`.
///
/// The frame is optionally downscaled during decode so preview and analysis work do not pay the
/// cost of full-resolution images when they do not need them.
pub fn extract_frame_image(
    video_path: &Path,
    time_ms: u64,
    max_width: Option<u32>,
) -> Result<DynamicImage> {
    let mut command = Command::new(ffmpeg_binary());
    command
        .arg("-loglevel")
        .arg("error")
        .arg("-nostdin")
        .arg("-i")
        .arg(video_path)
        .arg("-ss")
        .arg(format_seconds(time_ms));

    if let Some(max_width) = max_width {
        command
            .arg("-vf")
            .arg(format!("scale='min(iw,{max_width})':-1:flags=lanczos"));
    }

    let output = command
        .arg("-frames:v")
        .arg("1")
        .arg("-f")
        .arg("image2pipe")
        .arg("-vcodec")
        .arg("png")
        .arg("pipe:1")
        .output()
        .with_context(|| format!("failed to extract frame from {}", video_path.display()))?;

    if !output.status.success() {
        return Err(anyhow!(
            "ffmpeg failed to extract frame: {}",
            String::from_utf8_lossy(&output.stderr)
        ));
    }

    image::load_from_memory(&output.stdout).context("failed to decode extracted frame image")
}

/// Computes a cheap sharpness metric using variance of the Laplacian.
///
/// This is intentionally simple and deterministic for the first implementation pass.
pub fn laplacian_variance(image: &DynamicImage) -> f64 {
    let grayscale = image.thumbnail(320, 320).to_luma8();
    if grayscale.width() < 3 || grayscale.height() < 3 {
        return 0.0;
    }

    let mut values =
        Vec::with_capacity(((grayscale.width() - 2) * (grayscale.height() - 2)) as usize);
    for y in 1..grayscale.height() - 1 {
        for x in 1..grayscale.width() - 1 {
            let center = grayscale.get_pixel(x, y).0[0] as f64 * 4.0;
            let left = grayscale.get_pixel(x - 1, y).0[0] as f64;
            let right = grayscale.get_pixel(x + 1, y).0[0] as f64;
            let up = grayscale.get_pixel(x, y - 1).0[0] as f64;
            let down = grayscale.get_pixel(x, y + 1).0[0] as f64;
            values.push(center - left - right - up - down);
        }
    }

    let mean = values.iter().sum::<f64>() / values.len() as f64;
    values
        .iter()
        .map(|value| {
            let delta = value - mean;
            delta * delta
        })
        .sum::<f64>()
        / values.len() as f64
}

fn format_seconds(time_ms: u64) -> String {
    format!("{:.3}", time_ms as f64 / 1000.0)
}

fn parse_frame_rate(value: &str) -> Option<f64> {
    let parts: Vec<&str> = value.split('/').collect();
    match parts.as_slice() {
        [numerator, denominator] => {
            let numerator = numerator.parse::<f64>().ok()?;
            let denominator = denominator.parse::<f64>().ok()?;
            (denominator > 0.0).then_some(numerator / denominator)
        }
        [whole] => whole.parse::<f64>().ok(),
        _ => None,
    }
}

fn ffprobe_binary() -> &'static str {
    "ffprobe"
}

fn ffmpeg_binary() -> &'static str {
    "ffmpeg"
}

#[cfg(test)]
mod tests {
    use image::{DynamicImage, Rgb, RgbImage};

    use super::laplacian_variance;

    #[test]
    fn laplacian_variance_prefers_sharper_images() {
        let sharp = DynamicImage::ImageRgb8(RgbImage::from_fn(8, 8, |x, _| {
            if x < 4 {
                Rgb([0, 0, 0])
            } else {
                Rgb([255, 255, 255])
            }
        }));
        let soft = DynamicImage::ImageRgb8(RgbImage::from_fn(8, 8, |_, _| Rgb([127, 127, 127])));

        assert!(laplacian_variance(&sharp) > laplacian_variance(&soft));
    }
}
