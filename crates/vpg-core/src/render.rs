//! Contact-sheet rendering helpers shared by the desktop shell and the CLI.

use std::path::{Path, PathBuf};

use anyhow::{anyhow, Context, Result};
use base64::engine::general_purpose::STANDARD as BASE64_STANDARD;
use base64::Engine;
use font8x8::{UnicodeFonts, BASIC_FONTS};
use image::imageops::{overlay, resize, FilterType};
use image::{DynamicImage, ImageFormat, Rgba, RgbaImage};
use serde::{Deserialize, Serialize};

use crate::media::{effective_tile_time_ms, extract_frame_image};
use crate::project::{ExportFormat, ProjectFile, TileSelection};
use crate::seek::display_frame_number;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
/// Successful export metadata returned to the caller after rendering.
pub struct ExportResult {
    pub output_path: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
/// Downscaled preview of the rendered contact sheet used by the desktop editor.
pub struct SheetPreview {
    pub data_url: String,
    pub width: u32,
    pub height: u32,
}

/// Renders and writes the final contact sheet to disk.
pub fn export_sheet(project: &ProjectFile, output_override: Option<&str>) -> Result<ExportResult> {
    let output_path = resolve_output_path(project, output_override)?;
    let image = render_project(project, false)?;
    let format = match project.export.format {
        ExportFormat::Png => ImageFormat::Png,
        ExportFormat::Jpeg => ImageFormat::Jpeg,
    };

    image
        .save_with_format(&output_path, format)
        .with_context(|| format!("failed to save sheet to {}", output_path.display()))?;

    Ok(ExportResult {
        output_path: output_path.display().to_string(),
    })
}

/// Renders the current project into a PNG data URL sized for interactive preview.
pub fn render_preview(project: &ProjectFile, max_width: Option<u32>) -> Result<SheetPreview> {
    let rendered = render_project(project, true)?;
    let preview = if let Some(max_width) = max_width {
        if max_width > 0 && rendered.width() > max_width {
            DynamicImage::ImageRgba8(resize(
                &rendered.to_rgba8(),
                max_width,
                ((rendered.height() as f32 / rendered.width() as f32) * max_width as f32).round()
                    as u32,
                FilterType::Lanczos3,
            ))
        } else {
            rendered
        }
    } else {
        rendered
    };

    let mut bytes = Vec::new();
    preview
        .write_to(&mut std::io::Cursor::new(&mut bytes), ImageFormat::Png)
        .context("failed to encode sheet preview as PNG")?;

    Ok(SheetPreview {
        data_url: format!("data:image/png;base64,{}", BASE64_STANDARD.encode(bytes)),
        width: preview.width(),
        height: preview.height(),
    })
}

/// Renders the current project into an in-memory raster image.
///
/// The renderer is intentionally owned by Rust so the desktop app and CLI share identical
/// layout, timestamp, watermark, and styling behavior.
pub fn render_project(project: &ProjectFile, fast_seek: bool) -> Result<DynamicImage> {
    let scale = project.export.scale.max(0.25);
    let aspect_ratio = source_aspect_ratio(project);
    let (cell_width, cell_height) = derive_cell_size(scale, aspect_ratio);
    let gutter = ((project.grid.gutter_px as f32) * scale).round() as u32;
    let outer = ((project.grid.outer_margin_px as f32) * scale).round() as u32;
    let metadata_height = if project.style.show_metadata_bar {
        (72.0 * scale).round() as u32
    } else {
        0
    };
    let canvas_width =
        outer * 2 + project.grid.columns * cell_width + (project.grid.columns - 1) * gutter;
    let canvas_height = outer * 2
        + metadata_height
        + project.grid.rows * cell_height
        + (project.grid.rows - 1) * gutter;
    let background = if project.style.dark_mode {
        Rgba([16, 23, 34, 255])
    } else {
        Rgba([245, 247, 250, 255])
    };
    let mut canvas = RgbaImage::from_pixel(canvas_width, canvas_height, background);

    if project.style.show_metadata_bar {
        draw_filled_rect(
            &mut canvas,
            outer,
            outer,
            canvas_width - outer * 2,
            metadata_height,
            Rgba([29, 48, 64, 255]),
        );
        let metadata = format!(
            "{}  |  {} ms  |  {}x{}  |  {:.2} fps",
            Path::new(&project.video.path)
                .file_name()
                .and_then(|value| value.to_str())
                .unwrap_or(&project.video.path),
            project.video.duration_ms.unwrap_or_default(),
            project.video.width.unwrap_or_default(),
            project.video.height.unwrap_or_default(),
            project.video.fps.unwrap_or_default()
        );
        draw_text(
            &mut canvas,
            outer + 12,
            outer + 16,
            &metadata,
            Rgba([245, 247, 250, 255]),
            2,
        );
    }

    for tile in &project.tiles {
        let time_ms = effective_tile_time_ms(project, &tile.selection, tile.fine_tune_offset_ms);
        let x = outer + tile.span.column * (cell_width + gutter);
        let y = outer + metadata_height + tile.span.row * (cell_height + gutter);
        let width = tile.span.column_span * cell_width + (tile.span.column_span - 1) * gutter;
        let height = tile.span.row_span * cell_height + (tile.span.row_span - 1) * gutter;
        let frame = extract_frame_image(
            Path::new(&project.video.path),
            time_ms,
            Some(width.max(1)),
            fast_seek,
        )?;
        let tile_image = prepare_tile_image(project, &frame, width, height);

        if project.style.frame_shadow_px > 0 {
            draw_soft_shadow(
                &mut canvas,
                x + project.style.frame_shadow_px / 3,
                y + project.style.frame_shadow_px / 3,
                width,
                height,
                project.style.frame_rounding_px.min(width.min(height) / 4),
                project.style.frame_shadow_px,
            );
        }

        overlay(&mut canvas, &tile_image, i64::from(x), i64::from(y));

        if project.style.show_timestamps {
            let label = match tile.selection {
                TileSelection::ManualFrame { frame_index, .. } => {
                    format!("#{}", display_frame_number(frame_index, project.video.frame_count))
                }
                TileSelection::Auto => "AUTO".to_string(),
            };
            let timestamp = format!("{label}  {} ms", time_ms);
            let box_width = ((timestamp.len() as u32) * 9).max(80);
            let box_height = 20;
            let box_x = x + width.saturating_sub(box_width + 8);
            let box_y = y + height.saturating_sub(box_height + 8);
            draw_filled_rect(
                &mut canvas,
                box_x,
                box_y,
                box_width,
                box_height,
                Rgba([0, 0, 0, 180]),
            );
            draw_text(
                &mut canvas,
                box_x + 6,
                box_y + 6,
                &timestamp,
                Rgba([255, 255, 255, 255]),
                1,
            );
        }
    }

    if let Some(text) = project
        .watermark
        .text
        .as_ref()
        .filter(|text| !text.value.is_empty())
    {
        let color = Rgba([255, 255, 255, (text.opacity.clamp(0.0, 1.0) * 255.0) as u8]);
        draw_text(
            &mut canvas,
            canvas_width
                .saturating_sub((text.value.len() as u32) * 16)
                .saturating_sub(outer),
            canvas_height.saturating_sub(outer + 24),
            &text.value,
            color,
            2,
        );
    }

    if let Some(image_path) = project.watermark.image.as_ref() {
        if Path::new(&image_path.path).is_file() {
            let watermark = image::open(&image_path.path)
                .with_context(|| format!("failed to read watermark image {}", image_path.path))?;
            let watermark = resize(&watermark.to_rgba8(), 96, 96, FilterType::Lanczos3);
            overlay(
                &mut canvas,
                &watermark,
                i64::from(canvas_width.saturating_sub(outer + watermark.width())),
                i64::from(outer),
            );
        }
    }

    Ok(DynamicImage::ImageRgba8(canvas))
}

fn resolve_output_path(project: &ProjectFile, output_override: Option<&str>) -> Result<PathBuf> {
    if let Some(path) = output_override {
        return Ok(PathBuf::from(path));
    }
    if let Some(path) = project.export.output_path.as_deref() {
        return Ok(PathBuf::from(path));
    }

    let source = Path::new(&project.video.path);
    let stem = source
        .file_stem()
        .and_then(|value| value.to_str())
        .ok_or_else(|| anyhow!("video source path does not have a usable file name"))?;
    let extension = match project.export.format {
        ExportFormat::Png => "png",
        ExportFormat::Jpeg => "jpg",
    };
    let output_name = format!("{stem}_preview.{extension}");

    Ok(source
        .parent()
        .unwrap_or_else(|| Path::new("."))
        .join(output_name))
}

fn source_aspect_ratio(project: &ProjectFile) -> f32 {
    let width = project.video.width.unwrap_or(1920) as f32;
    let height = project.video.height.unwrap_or(1080) as f32;
    (width / height).max(0.25)
}

fn derive_cell_size(scale: f32, aspect_ratio: f32) -> (u32, u32) {
    let mut width = (240.0 * scale).round().max(96.0);
    let mut height = width / aspect_ratio;
    if height > 240.0 * scale {
        height = (240.0 * scale).round();
        width = height * aspect_ratio;
    }

    (width.round() as u32, height.round() as u32)
}

fn prepare_tile_image(
    project: &ProjectFile,
    frame: &DynamicImage,
    width: u32,
    height: u32,
) -> RgbaImage {
    let border = project.style.frame_border_px.max(1);
    let target_width = width.saturating_sub(border * 2).max(1);
    let target_height = height.saturating_sub(border * 2).max(1);
    let resized = resize(
        &frame.to_rgba8(),
        target_width,
        target_height,
        FilterType::Lanczos3,
    );
    let mut tile = RgbaImage::from_pixel(width, height, Rgba([255, 255, 255, 255]));
    overlay(&mut tile, &resized, i64::from(border), i64::from(border));
    apply_rounded_corners(
        &mut tile,
        project
            .style
            .frame_rounding_px
            .min(width.min(height) / 4)
            .max(1),
    );
    tile
}

fn draw_soft_shadow(
    image: &mut RgbaImage,
    x: u32,
    y: u32,
    width: u32,
    height: u32,
    radius: u32,
    blur_strength: u32,
) {
    let layers = blur_strength.max(1) / 4 + 1;
    for layer in 0..layers {
        let alpha = 40_u8.saturating_sub((layer * 6) as u8);
        let padding = layer * 2;
        // Layered rounded rectangles are cheap and predictable across platforms.
        draw_filled_rounded_rect(
            image,
            x.saturating_sub(padding),
            y.saturating_sub(padding),
            width + padding * 2,
            height + padding * 2,
            radius + padding,
            Rgba([0, 0, 0, alpha]),
        );
    }
}

fn draw_text(image: &mut RgbaImage, x: u32, y: u32, text: &str, color: Rgba<u8>, scale: u32) {
    for (index, character) in text.chars().enumerate() {
        if let Some(glyph) = BASIC_FONTS.get(character) {
            for (row, bits) in glyph.iter().enumerate() {
                for column in 0..8 {
                    if (bits >> column) & 1 == 1 {
                        for y_scale in 0..scale {
                            for x_scale in 0..scale {
                                let px = x
                                    + (index as u32 * 8 * scale)
                                    + (column as u32 * scale)
                                    + x_scale;
                                let py = y + (row as u32 * scale) + y_scale;
                                if px < image.width() && py < image.height() {
                                    image.put_pixel(px, py, color);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

fn draw_filled_rect(
    image: &mut RgbaImage,
    x: u32,
    y: u32,
    width: u32,
    height: u32,
    color: Rgba<u8>,
) {
    for py in y..(y + height).min(image.height()) {
        for px in x..(x + width).min(image.width()) {
            image.put_pixel(px, py, color);
        }
    }
}

fn draw_filled_rounded_rect(
    image: &mut RgbaImage,
    x: u32,
    y: u32,
    width: u32,
    height: u32,
    radius: u32,
    color: Rgba<u8>,
) {
    for py in y..(y + height).min(image.height()) {
        for px in x..(x + width).min(image.width()) {
            if point_in_rounded_rect(px - x, py - y, width, height, radius) {
                blend_pixel(image, px, py, color);
            }
        }
    }
}

fn apply_rounded_corners(image: &mut RgbaImage, radius: u32) {
    let width = image.width();
    let height = image.height();
    for y in 0..height {
        for x in 0..width {
            if !point_in_rounded_rect(x, y, width, height, radius) {
                image.get_pixel_mut(x, y).0[3] = 0;
            }
        }
    }
}

fn point_in_rounded_rect(x: u32, y: u32, width: u32, height: u32, radius: u32) -> bool {
    if radius == 0 {
        return true;
    }

    let radius = radius.min(width / 2).min(height / 2) as i64;
    let x = x as i64;
    let y = y as i64;
    let width = width as i64;
    let height = height as i64;

    let inside_horizontal = x >= radius && x < width - radius;
    let inside_vertical = y >= radius && y < height - radius;
    if inside_horizontal || inside_vertical {
        return true;
    }

    // Outside the center bands, fall back to a simple circle test for the active corner.
    let corner_center_x = if x < radius {
        radius
    } else {
        width - radius - 1
    };
    let corner_center_y = if y < radius {
        radius
    } else {
        height - radius - 1
    };
    let dx = x - corner_center_x;
    let dy = y - corner_center_y;
    dx * dx + dy * dy <= radius * radius
}

fn blend_pixel(image: &mut RgbaImage, x: u32, y: u32, color: Rgba<u8>) {
    let destination = image.get_pixel_mut(x, y);
    let alpha = color.0[3] as f32 / 255.0;
    for channel in 0..3 {
        destination.0[channel] = ((color.0[channel] as f32 * alpha)
            + (destination.0[channel] as f32 * (1.0 - alpha)))
            .round() as u8;
    }
    destination.0[3] = 255;
}
