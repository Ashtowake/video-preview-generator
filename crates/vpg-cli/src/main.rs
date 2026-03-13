use std::{fs, path::PathBuf};

use anyhow::{Context, Result};
use base64::engine::general_purpose::STANDARD as BASE64_STANDARD;
use base64::Engine;
use clap::{Parser, Subcommand};
use vpg_core::{
    assign_auto_tiles, validate_project, DiagnosticsBundle, MediaService, ProjectFile,
    ValidationLevel,
};

#[derive(Debug, Parser)]
#[command(name = "video-preview")]
#[command(about = "Headless tooling for the Video Preview Generator rewrite")]
struct Cli {
    #[command(subcommand)]
    command: Commands,
}

#[derive(Debug, Subcommand)]
enum Commands {
    Inspect {
        video: PathBuf,
    },
    PreviewProject {
        project: PathBuf,
        #[arg(long)]
        out: PathBuf,
        #[arg(long, default_value_t = 1200)]
        max_width: u32,
    },
    Preview {
        video: PathBuf,
        #[arg(long)]
        out: PathBuf,
        #[arg(long, default_value_t = 1200)]
        max_width: u32,
        #[arg(long)]
        crop_x: Option<f64>,
        #[arg(long)]
        crop_y: Option<f64>,
        #[arg(long)]
        crop_width: Option<f64>,
        #[arg(long)]
        crop_height: Option<f64>,
        #[arg(long)]
        range_start: Option<u64>,
        #[arg(long)]
        range_end: Option<u64>,
        #[arg(long)]
        sample_start: Option<u64>,
    },
    TimelineStrip {
        video: PathBuf,
        #[arg(long)]
        out: PathBuf,
        #[arg(long, default_value_t = 48)]
        count: u32,
        #[arg(long, default_value_t = 3456)]
        width: u32,
        #[arg(long, default_value_t = 84)]
        height: u32,
        #[arg(long)]
        crop_x: Option<f64>,
        #[arg(long)]
        crop_y: Option<f64>,
        #[arg(long)]
        crop_width: Option<f64>,
        #[arg(long)]
        crop_height: Option<f64>,
    },
    Export {
        project: PathBuf,
        #[arg(long)]
        out: Option<PathBuf>,
    },
    Sharpest {
        project: PathBuf,
        #[arg(long)]
        out: PathBuf,
        #[arg(required = true)]
        tile_ids: Vec<String>,
    },
    Batch {
        project: PathBuf,
    },
}

fn main() -> Result<()> {
    let cli = Cli::parse();

    match cli.command {
        Commands::Inspect { video } => inspect(video),
        Commands::PreviewProject {
            project,
            out,
            max_width,
        } => preview_project(project, out, max_width),
        Commands::Preview {
            video,
            out,
            max_width,
            crop_x,
            crop_y,
            crop_width,
            crop_height,
            range_start,
            range_end,
            sample_start,
        } => preview(
            video,
            out,
            max_width,
            crop_x,
            crop_y,
            crop_width,
            crop_height,
            range_start,
            range_end,
            sample_start,
        ),
        Commands::TimelineStrip {
            video,
            out,
            count,
            width,
            height,
            crop_x,
            crop_y,
            crop_width,
            crop_height,
        } => timeline_strip(
            video,
            out,
            count,
            width,
            height,
            crop_x,
            crop_y,
            crop_width,
            crop_height,
        ),
        Commands::Export { project, out } => export(project, out),
        Commands::Sharpest {
            project,
            out,
            tile_ids,
        } => sharpest(project, out, tile_ids),
        Commands::Batch { project } => batch(project),
    }
}

fn inspect(video: PathBuf) -> Result<()> {
    let service = MediaService;
    let project = service.load_video_project(&video.display().to_string())?;
    let estimate = service.estimate_full_fidelity(&project);
    let diagnostics = DiagnosticsBundle::from_project(&project, "./cache");

    println!(
        "{}",
        serde_json::to_string_pretty(&serde_json::json!({
            "video": project.video,
            "estimate": estimate,
            "diagnostics": diagnostics
        }))?
    );

    Ok(())
}

fn preview(
    video: PathBuf,
    out: PathBuf,
    max_width: u32,
    crop_x: Option<f64>,
    crop_y: Option<f64>,
    crop_width: Option<f64>,
    crop_height: Option<f64>,
    range_start: Option<u64>,
    range_end: Option<u64>,
    sample_start: Option<u64>,
) -> Result<()> {
    let service = MediaService;
    let mut project = service.load_video_project(&video.display().to_string())?;
    apply_optional_crop(&mut project, crop_x, crop_y, crop_width, crop_height)?;
    if range_start.is_some() || range_end.is_some() || sample_start.is_some() {
        let duration_ms = project.video.duration_ms.unwrap_or(project.range.end_ms.max(1));
        let start_ms = range_start.unwrap_or(project.range.start_ms).min(duration_ms.saturating_sub(1));
        let end_ms = range_end.unwrap_or(project.range.end_ms).min(duration_ms);
        if end_ms <= start_ms {
            anyhow::bail!("preview range requires --range-end greater than --range-start");
        }

        project.range.start_ms = start_ms;
        project.range.end_ms = end_ms;
        project.range.sample_start_ms = sample_start.unwrap_or(start_ms).clamp(start_ms, end_ms);
        assign_auto_tiles(
            &mut project.tiles,
            project.range.start_ms,
            project.range.end_ms,
            project.range.sample_start_ms,
            project.video.fps.unwrap_or(24.0),
            project.video.frame_count,
        );
    }

    let preview = service.render_preview(&project, Some(max_width))?;
    write_png_data_url(&preview.data_url, &out, "render preview did not return a PNG data URL")?;

    println!(
        "{}",
        serde_json::to_string_pretty(&serde_json::json!({
            "video": video,
            "output": out,
            "width": preview.width,
            "height": preview.height
        }))?
    );

    Ok(())
}

fn preview_project(project_path: PathBuf, out: PathBuf, max_width: u32) -> Result<()> {
    let service = MediaService;
    let project = read_project(&project_path)?;
    let preview = service.render_preview(&project, Some(max_width))?;
    write_png_data_url(&preview.data_url, &out, "render preview did not return a PNG data URL")?;

    println!(
        "{}",
        serde_json::to_string_pretty(&serde_json::json!({
            "project": project_path,
            "output": out,
            "width": preview.width,
            "height": preview.height
        }))?
    );

    Ok(())
}

fn timeline_strip(
    video: PathBuf,
    out: PathBuf,
    count: u32,
    width: u32,
    height: u32,
    crop_x: Option<f64>,
    crop_y: Option<f64>,
    crop_width: Option<f64>,
    crop_height: Option<f64>,
) -> Result<()> {
    let service = MediaService;
    let mut project = service.load_video_project(&video.display().to_string())?;
    apply_optional_crop(&mut project, crop_x, crop_y, crop_width, crop_height)?;
    let strip = service.render_timeline_strip(&project, count, width, height)?;
    write_png_data_url(
        &strip.data_url,
        &out,
        "render timeline strip did not return a PNG data URL",
    )?;

    println!(
        "{}",
        serde_json::to_string_pretty(&serde_json::json!({
            "video": video,
            "output": out,
            "width": strip.width,
            "height": strip.height,
            "count": count
        }))?
    );

    Ok(())
}

fn export(project_path: PathBuf, out: Option<PathBuf>) -> Result<()> {
    let service = MediaService;
    let project = read_project(&project_path)?;
    let issues = validate_project(&project);
    let errors: Vec<_> = issues
        .iter()
        .filter(|issue| issue.level == ValidationLevel::Error)
        .collect();

    if !errors.is_empty() {
        anyhow::bail!("project validation failed: {}", errors[0].message);
    }

    let render_plan = service.render_plan(&project);
    let export_result =
        service.export_sheet(&project, out.as_ref().and_then(|path| path.to_str()))?;
    println!(
        "{}",
        serde_json::to_string_pretty(&serde_json::json!({
            "project": project_path,
            "output": out,
            "renderPlan": render_plan,
            "exportResult": export_result
        }))?
    );

    Ok(())
}

fn sharpest(project_path: PathBuf, out: PathBuf, tile_ids: Vec<String>) -> Result<()> {
    let service = MediaService;
    let project = read_project(&project_path)?;
    let updated = service.find_sharpest_neighbours(&project, &tile_ids)?;
    fs::write(&out, serde_json::to_vec_pretty(&updated)?)
        .with_context(|| format!("failed to write updated project to {}", out.display()))?;

    println!(
        "{}",
        serde_json::to_string_pretty(&serde_json::json!({
            "project": project_path,
            "output": out,
            "updatedTiles": tile_ids
        }))?
    );

    Ok(())
}

fn batch(project_path: PathBuf) -> Result<()> {
    let project = read_project(&project_path)?;

    println!(
        "{}",
        serde_json::to_string_pretty(&serde_json::json!({
            "batchEnabled": project.batch.enabled,
            "inputs": project.batch.inputs,
            "retainManualOverrides": project.batch.retain_manual_overrides
        }))?
    );

    Ok(())
}

fn read_project(path: &PathBuf) -> Result<ProjectFile> {
    let contents = fs::read_to_string(path)
        .with_context(|| format!("failed to read project file {}", path.display()))?;
    let project = serde_json::from_str(&contents)
        .with_context(|| "failed to parse project JSON".to_string())?;
    Ok(project)
}

fn apply_optional_crop(
    project: &mut ProjectFile,
    crop_x: Option<f64>,
    crop_y: Option<f64>,
    crop_width: Option<f64>,
    crop_height: Option<f64>,
) -> Result<()> {
    let crop_arguments = [crop_x, crop_y, crop_width, crop_height];
    let crop_arguments_supplied = crop_arguments.iter().any(Option::is_some);
    if !crop_arguments_supplied {
        return Ok(());
    }

    let (Some(x), Some(y), Some(width), Some(height)) = (crop_x, crop_y, crop_width, crop_height)
    else {
        anyhow::bail!(
            "crop requires --crop-x, --crop-y, --crop-width, and --crop-height together"
        );
    };
    project.video.crop = Some(vpg_core::VideoCrop {
        x,
        y,
        width,
        height,
    });
    Ok(())
}

fn write_png_data_url(data_url: &str, out: &PathBuf, invalid_prefix_message: &str) -> Result<()> {
    let encoded = data_url
        .strip_prefix("data:image/png;base64,")
        .context(invalid_prefix_message.to_string())?;
    let bytes = BASE64_STANDARD
        .decode(encoded)
        .context("failed to decode preview PNG bytes")?;

    fs::write(out, bytes).with_context(|| format!("failed to write preview to {}", out.display()))
}
