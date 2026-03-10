use std::{fs, path::PathBuf};

use anyhow::{Context, Result};
use clap::{Parser, Subcommand};
use vpg_core::{validate_project, DiagnosticsBundle, MediaService, ProjectFile, ValidationLevel};

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
    Export {
        project: PathBuf,
        #[arg(long)]
        out: Option<PathBuf>,
    },
    Batch {
        project: PathBuf,
    },
}

fn main() -> Result<()> {
    let cli = Cli::parse();

    match cli.command {
        Commands::Inspect { video } => inspect(video),
        Commands::Export { project, out } => export(project, out),
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
