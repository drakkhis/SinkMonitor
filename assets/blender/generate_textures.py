#!/usr/bin/env python3
"""Generates high-resolution emissive screen and hazard stripe textures for SM_SinkMonitorTerminal."""

from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont

SCRIPT_DIR = Path(__file__).resolve().parent
FONT_BOLD = "/usr/share/fonts/liberation/LiberationSans-Bold.ttf"
FONT_REG = "/usr/share/fonts/liberation/LiberationSans-Regular.ttf"


def load_font(path: str, size: int) -> ImageFont.FreeTypeFont:
    try:
        return ImageFont.truetype(path, size)
    except OSError:
        return ImageFont.load_default()


def draw_text_right(
    draw: ImageDraw.ImageDraw,
    right_x: int,
    y: int,
    text: str,
    font: ImageFont.FreeTypeFont,
    fill: tuple[int, int, int],
) -> None:
    bbox = draw.textbbox((0, 0), text, font=font)
    width = bbox[2] - bbox[0]
    draw.text((right_x - width, y), text, font=font, fill=fill)


def draw_item_badge_icon(
    draw: ImageDraw.ImageDraw,
    cx: int,
    cy: int,
    radius: int,
    accent: tuple[int, int, int],
    kind: int,
) -> None:
    box = [cx - radius, cy - radius, cx + radius, cy + radius]
    draw.rounded_rectangle(box, radius=8, fill=(18, 28, 40), outline=accent, width=2)
    r = radius - 8
    if kind == 0:  # Rocket / nozzle
        pts = [(cx, cy - r), (cx + r // 2 + 2, cy + r // 2), (cx - r // 2 - 2, cy + r // 2)]
        draw.polygon(pts, fill=accent)
    elif kind == 1:  # Circuit diamond
        pts = [(cx, cy - r), (cx + r, cy), (cx, cy + r), (cx - r, cy)]
        draw.polygon(pts, outline=accent, width=3)
        draw.ellipse([cx - 5, cy - 5, cx + 5, cy + 5], fill=accent)
    elif kind == 2:  # Nuclear rings
        draw.ellipse([cx - r, cy - r, cx + r, cy + r], outline=accent, width=3)
        draw.ellipse([cx - 6, cy - 6, cx + 6, cy + 6], fill=accent)
    elif kind == 3:  # Rotor bars
        draw.rectangle([cx - r, cy - r, cx + r, cy + r], outline=accent, width=3)
        draw.line([(cx - r, cy), (cx + r, cy)], fill=accent, width=3)
        draw.line([(cx, cy - r), (cx, cy + r)], fill=accent, width=3)
    elif kind == 4:  # Frame box
        draw.rectangle([cx - r, cy - r, cx + r, cy + r], outline=accent, width=3)
        draw.rectangle([cx - r // 2, cy - r // 2, cx + r // 2, cy + r // 2], outline=accent, width=2)
    else:  # DNA capsule
        draw.rounded_rectangle(
            [cx - r // 2 - 2, cy - r, cx + r // 2 + 2, cy + r],
            radius=r // 2,
            outline=accent,
            width=3,
        )
        draw.line([(cx - r // 2, cy - 4), (cx + r // 2, cy + 4)], fill=accent, width=3)


def generate_screen_emissive(output_path: Path) -> None:
    width, height = 2048, 1024
    yy, xx = np.mgrid[0:height, 0:width]

    # Deep industrial navy-slate base with faint subtle grid and scanlines.
    img_arr = np.zeros((height, width, 3), dtype=np.uint8)
    img_arr[..., 0] = 8
    img_arr[..., 1] = 14
    img_arr[..., 2] = 22

    grid_mask = ((xx % 64) == 0) | ((yy % 64) == 0)
    img_arr[grid_mask] = (14, 24, 36)

    scan_mask = (yy % 4) == 0
    img_arr[scan_mask] = np.clip(img_arr[scan_mask].astype(np.int16) - 2, 0, 255).astype(np.uint8)

    img = Image.fromarray(img_arr, mode="RGB")
    draw = ImageDraw.Draw(img)

    orange = (250, 149, 73)
    amber = (255, 184, 77)
    cyan = (50, 213, 245)
    mint = (46, 232, 158)
    text_white = (236, 242, 248)
    text_muted = (138, 156, 176)
    panel_bg = (13, 21, 32)
    panel_border = (38, 56, 78)

    # Outer display bezel trim inside texture.
    draw.rectangle([16, 16, width - 17, height - 17], outline=(42, 62, 86), width=3)

    # Header bar
    draw.rounded_rectangle([32, 28, width - 32, 114], radius=8, fill=(16, 25, 38), outline=orange, width=2)
    draw.rounded_rectangle([46, 42, 62, 100], radius=3, fill=orange)

    f_title = load_font(FONT_BOLD, 34)
    f_sub = load_font(FONT_BOLD, 22)
    f_row_title = load_font(FONT_BOLD, 24)
    f_row_sub = load_font(FONT_REG, 19)
    f_rate = load_font(FONT_BOLD, 28)
    f_big = load_font(FONT_BOLD, 74)
    f_med = load_font(FONT_BOLD, 30)
    f_small = load_font(FONT_BOLD, 20)

    draw.text((80, 52), "FICSIT INC. // AWESOME SINK TELEMETRY TERMINAL", font=f_title, fill=text_white)

    # Status pill on top right
    draw.rounded_rectangle([1660, 44, 1994, 98], radius=24, fill=(14, 42, 38), outline=mint, width=2)
    draw.ellipse([1684, 61, 1704, 81], fill=mint)
    draw.text((1718, 57), "ONLINE  •  LINKED", font=f_sub, fill=mint)

    # Left Column: Live Item Rates
    left_x0, left_x1 = 32, 1140
    draw.rounded_rectangle([left_x0, 134, left_x1, 910], radius=10, fill=panel_bg, outline=panel_border, width=2)
    draw.text((56, 152), "LIVE SINK THROUGHPUT  (60s ROLLING)", font=f_sub, fill=orange)
    draw_text_right(draw, left_x1 - 28, 152, "RATE  /  MIN", f_sub, text_muted)
    draw.line([(52, 188), (left_x1 - 24, 188)], fill=panel_border, width=2)

    rows = [
        ("THERMAL PROPULSION ROCKET", "STANDARD  •  732,560 PTS/EA", "12.0 / min", 0.92, orange),
        ("ASSEMBLY DIRECTOR SYSTEM", "STANDARD  •  500,190 PTS/EA", "18.5 / min", 0.96, amber),
        ("NUCLEAR PASTA", "STANDARD  •  538,976 PTS/EA", "24.0 / min", 1.00, orange),
        ("TURBO MOTOR", "STANDARD  •  240,496 PTS/EA", "30.0 / min", 0.78, amber),
        ("HEAVY MODULAR FRAME", "STANDARD  •  10,800 PTS/EA", "60.0 / min", 0.56, orange),
        ("ALIEN DNA CAPSULE", "EXPLORATION  •  1,000+ PTS/EA", "4.5 / min", 0.40, cyan),
    ]

    row_top = 204
    row_h = 108
    row_gap = 10
    for idx, (name, subtitle, rate_str, frac, accent) in enumerate(rows):
        y0 = row_top + idx * (row_h + row_gap)
        y1 = y0 + row_h
        row_fill = (20, 31, 46) if idx == 0 else (16, 25, 38)
        row_outline = accent if idx == 0 else (30, 44, 64)
        draw.rounded_rectangle([52, y0, left_x1 - 24, y1], radius=8, fill=row_fill, outline=row_outline, width=2)

        draw_item_badge_icon(draw, 94, (y0 + y1) // 2, 26, accent, idx)
        draw.text((138, y0 + 14), name, font=f_row_title, fill=text_white)
        draw.text((138, y0 + 46), subtitle, font=f_row_sub, fill=text_muted)

        # Progress bar along lower portion of row
        bar_x0 = 138
        bar_x1 = left_x1 - 230
        bar_y0 = y0 + 78
        bar_y1 = y0 + 92
        draw.rounded_rectangle([bar_x0, bar_y0, bar_x1, bar_y1], radius=5, fill=(10, 16, 24))
        fill_x1 = bar_x0 + int((bar_x1 - bar_x0) * frac)
        draw.rounded_rectangle([bar_x0, bar_y0, fill_x1, bar_y1], radius=5, fill=accent)

        draw_text_right(draw, left_x1 - 44, y0 + 36, rate_str, f_rate, mint if accent != cyan else cyan)

    # Right Column: Summary KPIs & Sparkline
    right_x0, right_x1 = 1164, width - 32

    # Panel 1: Total Points/min
    draw.rounded_rectangle([right_x0, 134, right_x1, 344], radius=10, fill=panel_bg, outline=orange, width=2)
    draw.text((right_x0 + 26, 152), "GLOBAL POINTS / MIN", font=f_sub, fill=orange)
    draw_text_right(draw, right_x1 - 26, 152, "ALL TRACKS", f_small, text_muted)
    draw.text((right_x0 + 26, 198), "1,428,600", font=f_big, fill=text_white)
    draw_text_right(draw, right_x1 - 26, 246, "PTS / MIN", f_med, amber)
    draw.text((right_x0 + 26, 294), "SESSION TOTAL:  42,858,000 PTS", font=f_small, fill=mint)

    # Panel 2: Coupon Progress & Track Status
    draw.rounded_rectangle([right_x0, 364, right_x1, 584], radius=10, fill=panel_bg, outline=panel_border, width=2)
    draw.text((right_x0 + 26, 382), "NEXT FICSIT COUPON PROGRESS", font=f_sub, fill=text_white)
    draw_text_right(draw, right_x1 - 26, 382, "84%", f_med, amber)

    cp_x0, cp_x1 = right_x0 + 26, right_x1 - 26
    draw.rounded_rectangle([cp_x0, 428, cp_x1, 460], radius=8, fill=(10, 16, 24), outline=(46, 66, 90), width=2)
    draw.rounded_rectangle([cp_x0 + 4, 432, cp_x0 + int((cp_x1 - cp_x0 - 8) * 0.84), 456], radius=6, fill=orange)

    mid_x = (right_x0 + right_x1) // 2
    draw.rounded_rectangle([right_x0 + 26, 486, mid_x - 10, 560], radius=8, fill=(38, 25, 16), outline=orange, width=2)
    draw.text((right_x0 + 42, 498), "STANDARD TRACK", font=f_small, fill=orange)
    draw.text((right_x0 + 42, 526), "14,912 / MIN", font=f_sub, fill=text_white)

    draw.rounded_rectangle([mid_x + 10, 486, right_x1 - 26, 560], radius=8, fill=(14, 34, 44), outline=cyan, width=2)
    draw.text((mid_x + 26, 498), "EXPLORATION / DNA", font=f_small, fill=cyan)
    draw.text((mid_x + 26, 526), "4.5 / MIN", font=f_sub, fill=text_white)

    # Panel 3: 60s Rolling Throughput Waveform
    draw.rounded_rectangle([right_x0, 604, right_x1, 910], radius=10, fill=panel_bg, outline=panel_border, width=2)
    draw.text((right_x0 + 26, 622), "60s ROLLING THROUGHPUT WAVEFORM", font=f_sub, fill=cyan)
    draw_text_right(draw, right_x1 - 26, 622, "PEAK 1.49M", f_small, text_muted)

    gx0, gx1 = right_x0 + 28, right_x1 - 28
    gy0, gy1 = 664, 882
    draw.rectangle([gx0, gy0, gx1, gy1], fill=(9, 14, 22), outline=(32, 48, 68), width=2)
    for frac in (0.25, 0.50, 0.75):
        gy = int(gy0 + (gy1 - gy0) * frac)
        draw.line([(gx0 + 2, gy), (gx1 - 2, gy)], fill=(22, 34, 50), width=1)
    for step in range(1, 6):
        gx = int(gx0 + (gx1 - gx0) * (step / 6.0))
        draw.line([(gx, gy0 + 2), (gx, gy1 - 2)], fill=(22, 34, 50), width=1)

    samples = [
        0.48, 0.53, 0.50, 0.62, 0.68, 0.65, 0.71, 0.78, 0.74, 0.80,
        0.84, 0.79, 0.76, 0.82, 0.88, 0.86, 0.83, 0.89, 0.91, 0.87,
        0.85, 0.90, 0.93, 0.88, 0.86, 0.91,
    ]
    points = []
    for idx, val in enumerate(samples):
        px = int(gx0 + 10 + (gx1 - gx0 - 20) * (idx / (len(samples) - 1)))
        py = int(gy1 - 12 - (gy1 - gy0 - 28) * val)
        points.append((px, py))

    poly_points = [(points[0][0], gy1 - 4)] + points + [(points[-1][0], gy1 - 4)]
    draw.polygon(poly_points, fill=(18, 46, 58))
    draw.line(points, fill=cyan, width=4)
    for px, py in points[::4]:
        draw.ellipse([px - 5, py - 5, px + 5, py + 5], fill=orange, outline=text_white, width=1)

    # Bottom footer bar
    draw.rounded_rectangle([32, 928, width - 32, 992], radius=8, fill=(16, 25, 38), outline=panel_border, width=2)
    footer_text = "ACTIVE SINKS: 06   |   60s WINDOW   |   PRESS [E] TO ACCESS TELEMETRY"
    bbox = draw.textbbox((0, 0), footer_text, font=f_sub)
    fw = bbox[2] - bbox[0]
    draw.text(((width - fw) // 2, 948), footer_text, font=f_sub, fill=amber)

    img.save(output_path, format="PNG")
    print(f"Saved screen texture: {output_path} ({width}x{height})")


def generate_hazard_texture(output_path: Path) -> None:
    width, height = 2048, 512
    yy, xx = np.mgrid[0:height, 0:width]

    # 45-degree diagonal stripes repeating cleanly every 128 pixels across X.
    period = 128
    stripe_phase = (xx + yy) % period
    is_yellow = stripe_phase < (period // 2)

    img_arr = np.zeros((height, width, 3), dtype=np.uint8)
    img_arr[~is_yellow] = (26, 29, 33)    # #1A1D21 charcoal black
    img_arr[is_yellow] = (242, 183, 5)    # #F2B705 caution yellow

    # Thin metallic gunmetal edge trim top & bottom.
    trim_h = 28
    img_arr[:trim_h, :] = (37, 42, 48)
    img_arr[-trim_h:, :] = (37, 42, 48)
    img_arr[trim_h : trim_h + 4, :] = (120, 128, 138)
    img_arr[-trim_h - 4 : -trim_h, :] = (120, 128, 138)

    img = Image.fromarray(img_arr, mode="RGB")
    img.save(output_path, format="PNG")
    print(f"Saved hazard texture: {output_path} ({width}x{height})")


if __name__ == "__main__":
    generate_screen_emissive(SCRIPT_DIR / "T_SinkMonitor_Screen_Emissive.png")
    generate_hazard_texture(SCRIPT_DIR / "T_SinkMonitor_Hazard.png")
