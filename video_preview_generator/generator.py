import os
import cv2
from PIL import Image, ImageDraw, ImageFont, ImageOps
from pathlib import Path
from typing import Optional, Tuple

META_FONT_SIZE = 14
TS_FONT_SIZE = 14
DEFAULT_MAX_WIDTH = 2560
DEFAULT_MAX_HEIGHT = 1440
TS_PADDING = 2

def format_timestamp(seconds: float) -> str:
    h = int(seconds // 3600)
    m = int((seconds % 3600) // 60)
    s = int(seconds % 60)
    return f"{h:02d}:{m:02d}:{s:02d}"

def load_font(size: int, bold: bool = True, font_path: Optional[str] = None) -> ImageFont.FreeTypeFont:
    candidates = [font_path] if font_path and os.path.isfile(font_path) else []
    if bold:
        candidates += ["arialbd.ttf", "DejaVuSans-Bold.ttf"]
    else:
        candidates += ["arial.ttf", "DejaVuSans.ttf"]
    for c in candidates:
        try:
            return ImageFont.truetype(c, size)
        except Exception:
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
    font_path: Optional[str] = None,
    logo_path: Optional[str] = None,
    logo_opacity: float = 1.0,
    watermark_text: Optional[str] = None,
    watermark_font_size: int = 25,
    watermark_text_opacity: float = 1.0
):
    if not os.path.isfile(video_path):
        raise FileNotFoundError(f"{video_path} not found")
    cap = cv2.VideoCapture(video_path)
    if not cap.isOpened():
        raise RuntimeError("Cannot open video")
    fps = cap.get(cv2.CAP_PROP_FPS)
    count = cap.get(cv2.CAP_PROP_FRAME_COUNT)
    duration = count / fps if fps else 0
    orig_w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    orig_h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    size_mb = os.path.getsize(video_path) / (1024*1024)

    # compute sizes
    max_w, max_h = int(max_width*scale_factor), int(max_height*scale_factor)
    pad, bdr, outer = int(padding*scale_factor), int(border_size*scale_factor), int(outer_margin*scale_factor)
    font_meta = load_font(int(META_FONT_SIZE*scale_factor), True, font_path)
    font_ts   = load_font(int(TS_FONT_SIZE*scale_factor), True, font_path)
    font_wm   = load_font(int(watermark_font_size*scale_factor), True, font_path)

    # metadata block height
    ascent, descent = font_meta.getmetrics() if hasattr(font_meta, 'getmetrics') else (font_meta.getsize("H")[1],0)
    line_h = ascent + descent
    meta_height = pad + 4*(line_h+pad) + pad

    # thumbnail dims
    total_hpad = (cols+1)*pad + 2*bdr*cols + 2*outer
    total_vpad = meta_height + (rows+1)*pad + 2*bdr*rows + 2*outer
    thumb_max_w = (max_w - total_hpad)//cols
    thumb_max_h = (max_h - total_vpad)//rows
    scale_thumb = min(thumb_max_w/orig_w, thumb_max_h/orig_h, 1.0)
    tw, th = int(orig_w*scale_thumb), int(orig_h*scale_thumb)

    sheet_w = cols*(tw+2*bdr)+(cols+1)*pad+2*outer
    sheet_h = meta_height + rows*(th+2*bdr)+(rows+1)*pad+2*outer

    sheet = Image.new("RGBA", (sheet_w, sheet_h), (46,46,46,255))
    draw = ImageDraw.Draw(sheet)
    # draw metadata background
    draw.rectangle([outer,outer,sheet_w-outer,outer+meta_height], fill=(46,46,46,255))
    # draw metadata text
    meta = [
        ("File Name", Path(video_path).name),
        ("Size", f"{size_mb:.2f} MB"),
        ("Resolution", f"{orig_w}×{orig_h} @ {fps:.2f} fps"),
        ("Duration", format_timestamp(duration))
    ]
    key_w = max(draw.textbbox((0,0), f"{k}:", font=font_meta)[2] for k,_ in meta)
    x0, y = outer+pad, outer+pad
    for k,v in meta:
        draw.text((x0,y), f"{k}:", font=font_meta, fill=(255,255,255))
        draw.text((x0+key_w+pad,y), v, font=font_meta, fill=(255,255,255))
        y += line_h+pad

    # optional watermark/logo
    if logo_path and os.path.isfile(logo_path):
        logo = Image.open(logo_path).convert("RGBA")
        scale_l = (meta_height-2*pad)/logo.height
        logo = logo.resize((int(logo.width*scale_l),int(logo.height*scale_l)), Image.LANCZOS)
        alpha = logo.split()[3].point(lambda p: int(p*logo_opacity))
        logo.putalpha(alpha)
        xL = sheet_w-outer-logo.width
        yL = outer + (meta_height//2) - (logo.height//2)
        sheet.paste(logo, (xL,yL), logo)
    if watermark_text:
        layer = Image.new("RGBA", sheet.size)
        td = ImageDraw.Draw(layer)
        txt = watermark_text.upper()
        w, h = td.textbbox((0,0), txt, font=font_wm)[2:]
        xT = (xL - pad) if logo_path else (sheet_w-outer)
        td.text((xT, outer+(meta_height//2)), txt, font=font_wm,
                fill=(255,255,255,int(255*watermark_text_opacity)),
                anchor="rm")
        sheet = Image.alpha_composite(sheet, layer)

    # thumbnails + timestamps
    for i in range(cols*rows):
        t = (i+0.5)*duration/(cols*rows)
        cap.set(cv2.CAP_PROP_POS_FRAMES, int(t*fps))
        ok, frame = cap.read()
        if not ok: continue
        thumb = Image.fromarray(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))
        thumb = thumb.resize((tw,th), Image.LANCZOS)
        thumb = ImageOps.expand(thumb, border=bdr, fill="white")
        col_i, row_i = i%cols, i//cols
        x = outer + pad + col_i*(tw+2*bdr+pad)
        y = outer + meta_height + pad + row_i*(th+2*bdr+pad)
        sheet.paste(thumb, (x,y))
        ts = format_timestamp(t)
        bb = draw.textbbox((0,0), ts, font=font_ts)
        tw2, th2 = bb[2]-bb[0], bb[3]-bb[1]
        tx, ty = x+tw-tw2-pad, y+pad
        draw.rectangle([tx-TS_PADDING,ty-TS_PADDING,tx+tw2+TS_PADDING,ty+th2+TS_PADDING], fill=(0,0,0,180))
        draw.text((tx,y+pad/2), ts, font=font_ts, fill=(255,255,255))

    cap.release()
    sheet.convert("RGB").save(output_path, "PNG")
