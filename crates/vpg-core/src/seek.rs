use anyhow::{anyhow, Result};

/// Evenly distributes sample times across a range using center-of-bin spacing.
pub fn center_of_bin_samples(range_start_ms: u64, range_end_ms: u64, count: usize) -> Vec<u64> {
    if count == 0 || range_end_ms <= range_start_ms {
        return Vec::new();
    }

    let span = (range_end_ms - range_start_ms) as f64;

    (0..count)
        .map(|index| {
            let center = (index as f64 + 0.5) * span / count as f64;
            range_start_ms + center.round() as u64
        })
        .collect()
}

/// Evenly distributes sample times from an explicit starting point to the end of the range.
pub fn evenly_spaced_samples_from_start(
    range_start_ms: u64,
    range_end_ms: u64,
    sample_start_ms: u64,
    count: usize,
) -> Vec<u64> {
    if count == 0 || range_end_ms <= range_start_ms {
        return Vec::new();
    }

    let safe_start = if count == 1 {
        sample_start_ms.clamp(range_start_ms, range_end_ms)
    } else {
        sample_start_ms.clamp(range_start_ms, range_end_ms.saturating_sub(1))
    };

    if count == 1 {
        return vec![safe_start];
    }

    let span = (range_end_ms - safe_start) as f64;
    (0..count)
        .map(|index| {
            let offset = index as f64 * span / (count.saturating_sub(1)) as f64;
            safe_start + offset.round() as u64
        })
        .collect()
}

/// Clamps a playhead to the available media duration.
pub fn clamp_playhead_ms(playhead_ms: i128, duration_ms: u64) -> u64 {
    playhead_ms.clamp(0, duration_ms as i128) as u64
}

/// Applies a time-based jump in milliseconds to a playhead.
pub fn step_by_time(playhead_ms: u64, delta_ms: i64, duration_ms: u64) -> u64 {
    clamp_playhead_ms(playhead_ms as i128 + delta_ms as i128, duration_ms)
}

/// Applies an exact frame step using the available frames per second.
pub fn step_by_frames(playhead_ms: u64, frames: i32, fps: f64, duration_ms: u64) -> u64 {
    if fps <= 0.0 {
        return playhead_ms;
    }

    let delta_ms = (frames as f64 / fps * 1000.0).round() as i64;
    step_by_time(playhead_ms, delta_ms, duration_ms)
}

/// Parses `h:m:s.ms`, `m:s`, or plain seconds into milliseconds.
pub fn parse_time_delta(input: &str) -> Result<u64> {
    let trimmed = input.trim();
    if trimmed.is_empty() {
        return Err(anyhow!("time delta cannot be empty"));
    }

    let parts: Vec<&str> = trimmed.split(':').collect();
    let (hours, minutes, seconds_text) = match parts.as_slice() {
        [seconds] => (0_u64, 0_u64, *seconds),
        [minutes, seconds] => (0_u64, minutes.parse()?, *seconds),
        [hours, minutes, seconds] => (hours.parse()?, minutes.parse()?, *seconds),
        _ => return Err(anyhow!("unsupported time delta format")),
    };

    let seconds = seconds_text.parse::<f64>()?;
    let total_ms = ((hours * 3600 + minutes * 60) as f64 + seconds) * 1000.0;
    Ok(total_ms.round() as u64)
}

#[cfg(test)]
mod tests {
    use super::{
        center_of_bin_samples, clamp_playhead_ms, evenly_spaced_samples_from_start,
        parse_time_delta, step_by_frames, step_by_time,
    };

    #[test]
    fn samples_use_center_of_bin_spacing() {
        let samples = center_of_bin_samples(0, 1000, 4);
        assert_eq!(samples, vec![125, 375, 625, 875]);
    }

    #[test]
    fn playhead_clamps_to_duration() {
        assert_eq!(clamp_playhead_ms(-50, 1000), 0);
        assert_eq!(clamp_playhead_ms(1_250, 1000), 1000);
    }

    #[test]
    fn samples_can_start_from_an_explicit_position() {
        let samples = evenly_spaced_samples_from_start(0, 1_000, 200, 4);
        assert_eq!(samples, vec![200, 467, 733, 1_000]);
    }

    #[test]
    fn time_step_respects_bounds() {
        assert_eq!(step_by_time(500, -1_000, 10_000), 0);
        assert_eq!(step_by_time(500, 5_000, 2_000), 2_000);
    }

    #[test]
    fn frame_step_uses_fps() {
        assert_eq!(step_by_frames(1_000, 24, 24.0, 5_000), 2_000);
    }

    #[test]
    fn parses_time_delta_variants() {
        assert_eq!(parse_time_delta("5").unwrap(), 5_000);
        assert_eq!(parse_time_delta("1:05").unwrap(), 65_000);
        assert_eq!(parse_time_delta("1:02:03.5").unwrap(), 3_723_500);
    }
}
