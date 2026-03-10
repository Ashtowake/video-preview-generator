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

    if project.playback.frame_step == 0 {
        issues.push(ValidationIssue {
            level: ValidationLevel::Error,
            path: "playback.frameStep".to_string(),
            message: "frameStep must be at least 1".to_string(),
        });
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
}
