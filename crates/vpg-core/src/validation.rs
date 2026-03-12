use crate::grid::validate_grid_spans;
use crate::project::{AnalysisMode, ProjectFile};

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ValidationLevel {
    Error,
    Warning,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ValidationIssue {
    pub level: ValidationLevel,
    pub path: String,
    pub message: String,
}

/// Validates cross-field project invariants.
pub fn validate_project(project: &ProjectFile) -> Vec<ValidationIssue> {
    let mut issues = Vec::new();

    if project.range.end_ms <= project.range.start_ms {
        issues.push(ValidationIssue {
            level: ValidationLevel::Error,
            path: "range".to_string(),
            message: "range end must be greater than range start".to_string(),
        });
    }

    if project.range.sample_start_ms < project.range.start_ms
        || project.range.sample_start_ms > project.range.end_ms
    {
        issues.push(ValidationIssue {
            level: ValidationLevel::Error,
            path: "range.sampleStartMs".to_string(),
            message: "sample start must stay inside the selected range".to_string(),
        });
    }

    if project.playback.frame_step == 0 {
        issues.push(ValidationIssue {
            level: ValidationLevel::Error,
            path: "playback.frameStep".to_string(),
            message: "frameStep must be at least 1".to_string(),
        });
    }

    if let Some(crop) = project.video.crop {
        if crop.width <= 0.0 || crop.width > 1.0 || crop.height <= 0.0 || crop.height > 1.0 {
            issues.push(ValidationIssue {
                level: ValidationLevel::Error,
                path: "video.crop".to_string(),
                message: "crop width and height must both be greater than 0 and at most 1"
                    .to_string(),
            });
        }

        if crop.x < 0.0 || crop.y < 0.0 || crop.x >= 1.0 || crop.y >= 1.0 {
            issues.push(ValidationIssue {
                level: ValidationLevel::Error,
                path: "video.crop".to_string(),
                message: "crop origin must stay inside the normalized video frame".to_string(),
            });
        }

        if crop.x + crop.width > 1.0 || crop.y + crop.height > 1.0 {
            issues.push(ValidationIssue {
                level: ValidationLevel::Error,
                path: "video.crop".to_string(),
                message: "crop rectangle must fit inside the normalized video frame".to_string(),
            });
        }
    }

    if project.analysis_mode == AnalysisMode::QuickPreview && project.playback.frame_step > 1 {
        issues.push(ValidationIssue {
            level: ValidationLevel::Warning,
            path: "analysisMode".to_string(),
            message: "frame stepping will remain approximate until full fidelity mode is enabled"
                .to_string(),
        });
    }

    issues.extend(
        validate_grid_spans(&project.tiles, project.grid.rows, project.grid.columns)
            .into_iter()
            .map(|issue| ValidationIssue {
                level: ValidationLevel::Error,
                path: format!("tiles.{}", issue.tile_id),
                message: issue.reason,
            }),
    );

    issues
}

#[cfg(test)]
mod tests {
    use crate::project::ProjectFile;

    use super::{validate_project, ValidationLevel};

    #[test]
    fn invalid_range_is_reported() {
        let mut project = ProjectFile::starter("movie.mp4");
        project.range.start_ms = 2_000;
        project.range.end_ms = 1_000;

        let issues = validate_project(&project);
        assert_eq!(issues[0].level, ValidationLevel::Error);
    }

    #[test]
    fn invalid_crop_is_reported() {
        let mut project = ProjectFile::starter("movie.mp4");
        project.video.crop = Some(crate::project::VideoCrop {
            x: 0.75,
            y: 0.2,
            width: 0.5,
            height: 0.5,
        });

        let issues = validate_project(&project);
        assert!(issues.iter().any(|issue| issue.path == "video.crop"));
    }
}
