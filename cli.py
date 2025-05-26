import sys
import click
from pathlib import Path
from video_preview_generator.generator import make_thumbnail_sheet

@click.command()
@click.argument("video", type=click.Path(exists=True))
@click.option("-o", "--output", type=click.Path(), default=None,
              help="Output PNG path (defaults to video name with .png).")
@click.option("--scale", type=float, default=1.0, help="Scale factor relative to 2560×1440.")
@click.option("--logo", "logo_path", type=click.Path(exists=True), default=None,
              help="Path to logo image.")
@click.option("--opacity", type=float, default=1.0, help="Logo opacity 0..1.")
@click.option("--watermark-text", default=None, help="Watermark text.")
@click.option("--font-size", "wm_size", type=int, default=25, help="Watermark font size.")
@click.option("--text-opacity", type=float, default=1.0, help="Watermark text opacity 0..1.")
def main(video, output, scale, logo_path, opacity, watermark_text, wm_size, text_opacity):
    """Generate a thumbnail sheet with optional logo and watermark."""
    if output is None:
        output = Path(video).with_suffix(".png")
    make_thumbnail_sheet(
        video, str(output),
        scale_factor=scale,
        logo_path=logo_path,
        logo_opacity=opacity,
        watermark_text=watermark_text,
        watermark_font_size=wm_size,
        watermark_text_opacity=text_opacity
    )
    click.echo(f"Saved preview to {output}")

if __name__ == "__main__":
    main()
