import os, sys, argparse, cv2
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageOps

# Adjustable constants
META_FONT_SIZE = 14       # metadata text size (base)
TS_FONT_SIZE   = 18       # timestamp text size (base)
watermark_font_size   = 25       # watermark text size (base)
DEFAULT_MAX_WIDTH  = 2560
DEFAULT_MAX_HEIGHT = 1440
TS_PADDING = 2            # padding around timestamp text background
# Optional hardcoded watermark font path
WATERMARK_FONT_PATH = None


def format_timestamp(seconds: float) -> str:
    h = int(seconds // 3600)
    m = int((seconds % 3600) // 60)
    s = int(seconds % 60)
    return f"{h:02d}:{m:02d}:{s:02d}"


def load_font(size: int, bold: bool = True, font_path: str = None) -> ImageFont.FreeTypeFont:
    candidates = []
    if font_path and os.path.isfile(font_path):
        candidates.append(font_path)
    if bold:
        candidates += ["arialbd.ttf", "Arial Bold.ttf", "DejaVuSans-Bold.ttf"]
    else:
        candidates += ["arial.ttf", "Arial.ttf", "DejaVuSans.ttf"]
    for c in candidates:
        try:
            return ImageFont.truetype(c, size)
        except:
            continue
    return ImageFont.load_default()


def make_thumbnail_sheet(
    video_path: str,
    output_path: str,
    cols: int = 5,
    rows: int = 4,
    max_width: int = DEFAULT_MAX_WIDTH,
    max_height: int = DEFAULT_MAX_HEIGHT,
    padding: int = 5,
    border_size: int = 1,
    outer_margin: int = 15,
    scale_factor: float = 1.0,
    font_path: str = None,
    logo_path: str = None,
    logo_opacity: float = 1.0,
    watermark_text: str = None,
    watermark_font_size: int = 25,
    watermark_text_opacity: float = 1.0
):
    # Validate video file
    if not os.path.isfile(video_path):
        raise FileNotFoundError(f"{video_path} not found")
    cap = cv2.VideoCapture(video_path)
    if not cap.isOpened():
        raise RuntimeError("Cannot open video")

    # Scale UI dimensions
    max_w = int(max_width * scale_factor)
    max_h = int(max_height * scale_factor)
    pad = int(padding * scale_factor)
    border = int(border_size * scale_factor)
    outer = int(outer_margin * scale_factor)

    # Video metadata
    fps = cap.get(cv2.CAP_PROP_FPS)
    frame_count = cap.get(cv2.CAP_PROP_FRAME_COUNT)
    duration = frame_count / fps
    orig_w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    orig_h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    size_mb = os.path.getsize(video_path) / (1024 * 1024)
    metadata = [
        ("File Name", os.path.basename(video_path)),
        ("File Size", f"{size_mb:.2f} MB"),
        ("Resolution", f"{orig_w}×{orig_h} @ {fps:.2f} fps"),
        ("Duration", format_timestamp(duration))
    ]

    # Load fonts
    font_meta = load_font(int(META_FONT_SIZE * scale_factor), True, font_path)
    font_ts   = load_font(int(TS_FONT_SIZE   * scale_factor), True, font_path)
    wm_font_path = WATERMARK_FONT_PATH or font_path
    font_wm   = load_font(int(watermark_font_size   * scale_factor), True, wm_font_path)

    # Compute metadata bar height
    if hasattr(font_meta, 'getmetrics'):
        ascent, descent = font_meta.getmetrics()
    else:
        ascent, descent = font_meta.getsize("Hg")[1], 0
    line_h = ascent + descent
    meta_height = pad + len(metadata) * (line_h + pad) + pad

    # Compute thumbnail sizes
    total_hpad = (cols + 1) * pad + 2 * border * cols + 2 * outer
    total_vpad = meta_height + (rows + 1) * pad + 2 * border * rows + 2 * outer
    thumb_max_w = (max_w - total_hpad) // cols
    thumb_max_h = (max_h - total_vpad) // rows
    scale_thumb = min(thumb_max_w / orig_w, thumb_max_h / orig_h, 1.0)
    thumb_w, thumb_h = int(orig_w * scale_thumb), int(orig_h * scale_thumb)

    # Sheet dimensions
    sheet_w = cols * (thumb_w + 2 * border) + (cols + 1) * pad + 2 * outer
    sheet_h = meta_height + rows * (thumb_h + 2 * border) + (rows + 1) * pad + 2 * outer

    # Create RGBA canvas for overlay
    sheet = Image.new("RGBA", (sheet_w, sheet_h), (46,46,46,255))
    draw = ImageDraw.Draw(sheet)

    # Draw metadata background
    draw.rectangle(
        [outer, outer, sheet_w - outer, outer + meta_height],
        fill=(46,46,46,255)
    )

    # Prepare watermark text and logo
    text = watermark_text.upper() if watermark_text else None

    logo_img = None
    if logo_path and os.path.isfile(logo_path):
        logo_img = Image.open(logo_path).convert("RGBA")
        logo_scale = (meta_height - 2*pad) / logo_img.height
        logo_w = int(logo_img.width * logo_scale)
        logo_h = int(logo_img.height * logo_scale)
        logo_img = logo_img.resize((logo_w, logo_h), Image.LANCZOS)
        alpha = logo_img.split()[3].point(lambda p: int(p * logo_opacity))
        logo_img.putalpha(alpha)

    # compute the horizontal centre as before
    ## group_x = outer + (sheet_w - 2*outer) // 2

    # compute the vertical “middle of the text‐block” rather than the middle of the bar
    group_y = outer + (meta_height // 2) - (pad // 2)

    # 1) Paste the logo (right‐aligned, centre on adjusted group_y)
    if logo_img:
        x_logo = sheet_w - outer - logo_w - pad
        y_logo = group_y - (logo_h // 2)
        sheet.paste(logo_img, (x_logo, y_logo), logo_img)

    # 2) Draw the watermark text (right‐aligned, vertically centred on the same group_y)
    if text:
        alpha_text = int(255 * watermark_text_opacity)
        layer = Image.new("RGBA", sheet.size)
        td    = ImageDraw.Draw(layer)

        # if there's a logo, push the text pad pixels to its left; otherwise flush to the margin
        x_text = (x_logo - pad*2) if logo_img else (sheet_w - outer)

        td.text(
            (x_text, group_y),
            text,
            font=font_wm,
            fill=(255,255,255,alpha_text),
            anchor="rm"      # right‐middle
        )

        sheet = Image.alpha_composite(sheet, layer)

    draw = ImageDraw.Draw(sheet)

    key_widths = [draw.textbbox((0,0), f"{k}:", font=font_meta)[2] for k,_ in metadata]
    indent_x = outer + pad + max(key_widths) + pad
    y_text = outer + pad
    for key, value in metadata:
        draw.text((outer+pad, y_text), f"{key}:", fill=(255,255,255,255), font=font_meta)
        draw.text((indent_x, y_text), value, fill=(255,255,255,255), font=font_meta)
        y_text += line_h + pad

    # Place thumbnails and timestamps
    for idx in range(cols * rows):
        t = (idx + 0.5) * duration / (cols * rows)
        cap.set(cv2.CAP_PROP_POS_FRAMES, int(t * fps))
        ok, frame = cap.read()
        if not ok:
            continue
        thumb = Image.fromarray(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)).resize((thumb_w, thumb_h), Image.LANCZOS)
        thumb = ImageOps.expand(thumb, border=border, fill='white')
        col_i, row_i = idx % cols, idx // cols
        px = outer + pad + col_i*(thumb_w+2*border+pad)
        py = outer+meta_height+pad + row_i*(thumb_h+2*border+pad)
        sheet.paste(thumb, (px, py))
        # Timestamp
        ts = format_timestamp(t)
        bb2 = draw.textbbox((0,0), ts, font=font_ts)
        tw, th = bb2[2]-bb2[0], bb2[3]-bb2[1]
        tx, ty = px+border+thumb_w-tw-pad, py+pad
        draw.rectangle([tx-TS_PADDING, ty-TS_PADDING, tx+tw+TS_PADDING, ty+th+TS_PADDING], fill=(0,0,0,180))
        draw.text((tx, ty-pad/2), ts, fill=(255,255,255,255), font=font_ts)

    cap.release()
    # Convert back to RGB and save
    sheet.convert("RGB").save(output_path, "PNG")


def main():
    parser = argparse.ArgumentParser(description="Generate a video thumbnail sheet with optional logo and watermark text")
    parser.add_argument("video", help="Video file path")
    parser.add_argument("-o", "--output", default=None, help="Output PNG path")
    parser.add_argument("--font", default=None, help="Path to TTF font for all text")
    parser.add_argument("--scale", type=float, default=1.0, help="Scale factor relative to 2560×1440 = 1.0")
    parser.add_argument("--logo-path", dest="logo_path", default=None, help="Path to logo PNG")
    parser.add_argument("--logo-opacity", dest="logo_opacity", type=float, default=1, help="Logo opacity 0..1")
    parser.add_argument("--watermark-text", dest="watermark_text", default=None, help="Text watermark to display")
    parser.add_argument("--font-size", dest="watermark_font_size", default=25, help="Text watermark font size")
    parser.add_argument("--text-opacity", dest="watermark_text_opacity", type=float, default=1.0, help="Text watermark opacity 0..1")
    args = parser.parse_args()

    if args.output is None:
        # using pathlib for clarity
        args.output = Path(args.video).with_suffix(".png")

    make_thumbnail_sheet(
        args.video,
        args.output,
        padding=5,
        border_size=1,
        outer_margin=15,
        scale_factor=args.scale,
        font_path=args.font,
        logo_path=args.logo_path,
        logo_opacity=args.logo_opacity,
        watermark_text=args.watermark_text,
        watermark_font_size=int(args.watermark_font_size),
        watermark_text_opacity=args.watermark_text_opacity
    )

if __name__ == "__main__":
    main()