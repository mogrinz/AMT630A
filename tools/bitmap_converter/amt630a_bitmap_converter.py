#!/usr/bin/env python3
"""Convert a static image/GIF frame into an AMT630A_OSD::Bitmap header.

Default output targets LargeFont-compatible 16x22 bitmap tiles so bitmap windows
can coexist with normal LargeFont ROM text without changing FB76/FB77.
"""

from __future__ import annotations

import argparse
import json
import warnings
import math
import re
import sys
from pathlib import Path
from collections import Counter
from typing import Iterable, List, Sequence, Tuple

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow is required. Install with: pip install Pillow", file=sys.stderr)
    sys.exit(2)

warnings.simplefilter("ignore", DeprecationWarning)

RGB = Tuple[int, int, int]


def sanitize_name(name: str) -> str:
    name = re.sub(r"[^0-9A-Za-z_]", "_", name)
    if not name:
        name = "bitmap"
    if name[0].isdigit():
        name = "bitmap_" + name
    return name


# ---------------------------------------------------------------------------
# AMT630A display calibration
#
# Hardware measurements on the target LCD/AMT630A combination found that the
# palette DAC/display response is strongly nonlinear.  These measured points
# describe the AMT nibble that visually matched black/white dither references:
#
#   desired  0% -> code 0.0
#   desired 25% -> code 2.5
#   desired 50% -> code 3.5
#   desired 75% -> code 5.5
#   desired100% -> code15.0
#
# We interpolate between these points independently for R, G and B.
#
# The forward curve converts an ordinary desired sRGB channel value into the
# AMT nibble to write.  The inverse curve predicts the displayed brightness of
# an AMT nibble and is used for palette optimization and preview generation.
# ---------------------------------------------------------------------------

_CAL_DESIRED = (0.0, 0.25, 0.50, 0.75, 1.0)
_CAL_CODE    = (0.0, 2.50, 3.50, 5.50, 15.0)


def _interp_piecewise(x: float,
                      xs: Sequence[float],
                      ys: Sequence[float]) -> float:
    if x <= xs[0]:
        return ys[0]
    if x >= xs[-1]:
        return ys[-1]
    for i in range(len(xs) - 1):
        if x <= xs[i + 1]:
            span = xs[i + 1] - xs[i]
            t = 0.0 if span == 0 else (x - xs[i]) / span
            return ys[i] + t * (ys[i + 1] - ys[i])
    return ys[-1]


def desired8_to_amt_nibble(v: int) -> int:
    """Map desired 8-bit channel brightness to calibrated AMT 4-bit code."""
    desired = max(0.0, min(1.0, v / 255.0))
    code = _interp_piecewise(desired, _CAL_DESIRED, _CAL_CODE)
    # Explicit half-up rounding avoids Python's bankers-rounding surprises.
    return max(0, min(15, int(math.floor(code + 0.5))))


def amt_nibble_to_display8(code: int) -> int:
    """Predict displayed 8-bit brightness for one AMT 4-bit channel code."""
    code_f = float(max(0, min(15, code)))
    desired = _interp_piecewise(code_f, _CAL_CODE, _CAL_DESIRED)
    return max(0, min(255, int(math.floor(desired * 255.0 + 0.5))))


def rgb_to_amt(rgb: RGB) -> int:
    """Convert desired RGB888 to calibrated AMT BBBB GGGG RRRR."""
    r, g, b = rgb
    r4 = desired8_to_amt_nibble(r)
    g4 = desired8_to_amt_nibble(g)
    b4 = desired8_to_amt_nibble(b)
    return (b4 << 8) | (g4 << 4) | r4


def amt_to_rgb(value: int) -> RGB:
    """Predict displayed RGB888 for a raw AMT BBBB GGGG RRRR value."""
    r4 = value & 0xF
    g4 = (value >> 4) & 0xF
    b4 = (value >> 8) & 0xF
    return (
        amt_nibble_to_display8(r4),
        amt_nibble_to_display8(g4),
        amt_nibble_to_display8(b4),
    )


def rgb8_to_rgb4(rgb: RGB) -> Tuple[int, int, int]:
    """Convert desired RGB888 to calibrated AMT channel codes."""
    r, g, b = rgb
    return (
        desired8_to_amt_nibble(r),
        desired8_to_amt_nibble(g),
        desired8_to_amt_nibble(b),
    )


def rgb4_to_rgb8(rgb4: Tuple[int, int, int]) -> RGB:
    """Predict displayed RGB888 for calibrated AMT channel codes."""
    r, g, b = rgb4
    return (
        amt_nibble_to_display8(r),
        amt_nibble_to_display8(g),
        amt_nibble_to_display8(b),
    )


def _srgb_channel_to_linear(v: float) -> float:
    v /= 255.0
    if v <= 0.04045:
        return v / 12.92
    return ((v + 0.055) / 1.055) ** 2.4


def _rgb_to_lab(rgb: RGB) -> Tuple[float, float, float]:
    """Convert sRGB888 to CIE Lab (D65). Used only for palette optimization."""
    r = _srgb_channel_to_linear(rgb[0])
    g = _srgb_channel_to_linear(rgb[1])
    b = _srgb_channel_to_linear(rgb[2])

    x = (0.4124564 * r + 0.3575761 * g + 0.1804375 * b) / 0.95047
    y = (0.2126729 * r + 0.7151522 * g + 0.0721750 * b)
    z = (0.0193339 * r + 0.1191920 * g + 0.9503041 * b) / 1.08883

    delta = 6.0 / 29.0
    d3 = delta ** 3
    def f(t: float) -> float:
        if t > d3:
            return t ** (1.0 / 3.0)
        return t / (3.0 * delta * delta) + 4.0 / 29.0

    fx, fy, fz = f(x), f(y), f(z)
    return (116.0 * fy - 16.0,
            500.0 * (fx - fy),
            200.0 * (fy - fz))


def _lab_dist2(a: Tuple[float, float, float], b: Tuple[float, float, float]) -> float:
    return ((a[0] - b[0]) ** 2 +
            (a[1] - b[1]) ** 2 +
            (a[2] - b[2]) ** 2)


def _amt4_lab(rgb4: Tuple[int, int, int]) -> Tuple[float, float, float]:
    return _rgb_to_lab(rgb4_to_rgb8(rgb4))


def quantize_amt_palette(opaque_pixels: Sequence[RGB], color_count: int) -> List[RGB]:
    """Choose an efficient palette directly in the AMT RGB444 color space.

    v0.5.4 strategy:
      * Map every source pixel through the measured AMT display calibration
        curve before selecting a hardware-representable palette color.
      * Never produce duplicate RGB444 palette entries.
      * Use every requested slot when enough distinct source colors exist.
      * Add each new color where it reduces the largest remaining weighted
        perceptual error, rather than reserving slots for any specific hue family.
      * Refine the palette with weighted perceptual centroids, always selecting
        actual calibrated AMT codes as the final entries.

    This naturally spends spare entries on grays when gray detail is where the
    image still has the most error, but behaves the same way for any other image.
    """
    if not opaque_pixels:
        return [(0, 0, 0)]

    hist = Counter(rgb8_to_rgb4(px) for px in opaque_pixels)
    points = list(hist.keys())

    if len(points) <= color_count:
        points.sort(key=lambda p: (-hist[p], p))
        return [rgb4_to_rgb8(p) for p in points]

    labs = {p: _amt4_lab(p) for p in points}

    # First center: the most common representable AMT color.
    centers: List[Tuple[int, int, int]] = [
        max(points, key=lambda p: (hist[p], p))
    ]

    # Greedily spend every remaining slot where the current palette is causing
    # the most TOTAL image error. A moderately different common shade will beat
    # a very unusual one-pixel outlier.
    while len(centers) < color_count:
        best_point = None
        best_score = -1.0
        for p in points:
            if p in centers:
                continue
            d2 = min(_lab_dist2(labs[p], labs[c]) for c in centers)
            score = d2 * hist[p]
            if score > best_score:
                best_score = score
                best_point = p
        if best_point is None:
            break
        centers.append(best_point)

    # Refine. We compute the weighted Lab centroid of each cluster, then choose
    # the actual RGB444 source color in that cluster nearest the centroid. This
    # keeps every palette entry hardware-representable and avoids post-quantize
    # collapse from RGB888 to RGB444.
    for _ in range(12):
        buckets: List[List[Tuple[int, int, int]]] = [[] for _ in centers]
        for p in points:
            idx = min(range(len(centers)),
                      key=lambda i: (_lab_dist2(labs[p], labs[centers[i]]), i))
            buckets[idx].append(p)

        proposed: List[Tuple[int, int, int]] = []
        used = set()
        for i, bucket in enumerate(buckets):
            if not bucket:
                continue

            total = sum(hist[p] for p in bucket)
            centroid = tuple(
                sum(labs[p][ch] * hist[p] for p in bucket) / total
                for ch in range(3)
            )

            ranked = sorted(
                bucket,
                key=lambda p: (_lab_dist2(labs[p], centroid), -hist[p], p)
            )
            chosen = next((p for p in ranked if p not in used), None)
            if chosen is None:
                continue
            proposed.append(chosen)
            used.add(chosen)

        # Any collision/empty-cluster loses a center. Refill those slots using
        # the same largest-remaining-error rule, guaranteeing useful unique slots.
        while len(proposed) < color_count:
            best_point = None
            best_score = -1.0
            for p in points:
                if p in used:
                    continue
                if proposed:
                    d2 = min(_lab_dist2(labs[p], labs[c]) for c in proposed)
                else:
                    d2 = 1.0
                score = d2 * hist[p]
                if score > best_score:
                    best_score = score
                    best_point = p
            if best_point is None:
                break
            proposed.append(best_point)
            used.add(best_point)

        if proposed == centers:
            break
        centers = proposed

    # One final greedy repair pass guarantees all available slots are populated
    # whenever the source actually contains enough distinct AMT colors.
    dedup: List[Tuple[int, int, int]] = []
    for c in centers:
        if c not in dedup:
            dedup.append(c)
    while len(dedup) < min(color_count, len(points)):
        candidate = max(
            (p for p in points if p not in dedup),
            key=lambda p: (min(_lab_dist2(labs[p], labs[c]) for c in dedup) * hist[p],
                           hist[p], p)
        )
        dedup.append(candidate)

    return [rgb4_to_rgb8(c) for c in dedup[:color_count]]


def nearest_amt_index(rgb: RGB, palette: Sequence[RGB]) -> int:
    """Map a source pixel using the same RGB444 + Lab metric used by quantization."""
    snapped = rgb4_to_rgb8(rgb8_to_rgb4(rgb))
    lab = _rgb_to_lab(snapped)
    best_i = 0
    best_d = None
    for i, color in enumerate(palette):
        d = _lab_dist2(lab, _rgb_to_lab(color))
        if best_d is None or d < best_d:
            best_d = d
            best_i = i
    return best_i

def load_shared_palette(path: Path) -> Tuple[List[RGB], bool]:
    obj = json.loads(path.read_text(encoding="utf-8"))
    palette0_transparent = False
    if isinstance(obj, dict):
        palette0_transparent = bool(obj.get("palette0_transparent", False))
        obj = obj.get("colors", obj.get("palette"))
    if not isinstance(obj, list) or not obj:
        raise ValueError("palette JSON must contain a non-empty list")
    colors: List[RGB] = []
    for item in obj:
        if isinstance(item, str):
            s = item.strip().lstrip("#")
            if len(s) != 6:
                raise ValueError(f"invalid palette color: {item}")
            colors.append((int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16)))
        elif isinstance(item, list) and len(item) == 3:
            colors.append(tuple(max(0, min(255, int(v))) for v in item))  # type: ignore[arg-type]
        else:
            raise ValueError(f"invalid palette entry: {item!r}")
    return colors, palette0_transparent


def export_palette(path: Path, palette: Sequence[RGB], transparent0: bool) -> None:
    payload = {
        "format": "AMT630A bitmap palette",
        "palette0_transparent": transparent0,
        "colors": [f"#{r:02X}{g:02X}{b:02X}" for r, g, b in palette],
    }
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def write_header(
    path: Path,
    name: str,
    width: int,
    height: int,
    tile_w: int,
    tile_h: int,
    cols: int,
    rows: int,
    palette_amt: Sequence[int],
    transparent0: bool,
    words: Sequence[int],
) -> None:
    guard = sanitize_name(path.stem).upper() + "_H"
    lines: List[str] = []
    lines.append(f"#ifndef {guard}")
    lines.append(f"#define {guard}")
    lines.append("")
    lines.append("#include <AMT630A_OSD.h>")
    lines.append("")
    lines.append(f"// Generated by amt630a_bitmap_converter.py v0.5.4")
    lines.append(f"// Source size: {width}x{height}; tile grid: {cols}x{rows} of {tile_w}x{tile_h}")
    lines.append(f"// Font RAM storage: {len(words)} words / {len(words)*2} bytes")
    lines.append("")
    lines.append(f"static const uint16_t {name}_palette[16] PROGMEM = {{")
    padded_palette = list(palette_amt)[:16] + [0] * (16 - len(palette_amt))
    for i in range(0, 16, 8):
        lines.append("  " + ", ".join(f"0x{v:03X}" for v in padded_palette[i:i+8]) + ",")
    lines.append("};")
    lines.append("")
    lines.append(f"static const uint16_t {name}_data[{len(words)}] PROGMEM = {{")
    for i in range(0, len(words), 8):
        lines.append("  " + ", ".join(f"0x{v:04X}" for v in words[i:i+8]) + ",")
    lines.append("};")
    lines.append("")
    lines.append(f"static const AMT630A_OSD::Bitmap {name} = {{")
    lines.append(f"  {width}, {height},")
    lines.append(f"  {tile_w}, {tile_h},")
    lines.append(f"  {cols}, {rows},")
    lines.append(f"  {len(words)},")
    lines.append("  16,")
    lines.append(f"  {'true' if transparent0 else 'false'},")
    lines.append(f"  {name}_palette,")
    lines.append(f"  {name}_data")
    lines.append("};")
    lines.append("")
    lines.append(f"#endif // {guard}")
    lines.append("")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    ap = argparse.ArgumentParser(description="Convert an image/GIF to an AMT630A bitmap header")
    ap.add_argument("input", type=Path, help="input GIF/PNG/JPEG/BMP; GIF uses first frame")
    ap.add_argument("output", type=Path, help="output Arduino .h file")
    ap.add_argument("--name", help="C/C++ bitmap symbol name (default: output filename stem)")
    ap.add_argument("--font", choices=["large"], default="large",
                    help="target global font geometry; v0.5.1 supports large (16x22)")
    ap.add_argument("--max-bytes", type=int, default=8192,
                    help="reject output if packed Font RAM data exceeds this byte count (default 8192)")
    ap.add_argument("--max-colors", type=int, default=16,
                    help="maximum total palette entries including transparent index 0 (2..16)")
    ap.add_argument("--opaque", action="store_true",
                    help="do not reserve palette index 0 for transparent padding when source has no transparency")
    ap.add_argument("--palette", type=Path,
                    help="JSON shared palette; maps image to these colors instead of auto-quantizing")
    ap.add_argument("--export-palette", type=Path,
                    help="write the final RGB palette to JSON for reuse with other images")
    ap.add_argument("--preview", type=Path,
                    help="write PNG preview using the measured AMT display calibration")
    args = ap.parse_args()

    if not 2 <= args.max_colors <= 16:
        ap.error("--max-colors must be between 2 and 16")
    if args.max_bytes <= 0:
        ap.error("--max-bytes must be positive")

    img = Image.open(args.input)
    try:
        img.seek(0)
    except EOFError:
        pass
    rgba = img.convert("RGBA")
    width, height = rgba.size
    src = list(rgba.getdata())
    source_has_transparency = any(a < 128 for _, _, _, a in src)
    transparent0 = source_has_transparency or not args.opaque

    visible_slots = args.max_colors - (1 if transparent0 else 0)
    if visible_slots < 1:
        raise SystemExit("ERROR: no palette slots remain for visible colors")

    opaque_pixels: List[RGB] = [(r, g, b) for r, g, b, a in src if a >= 128]

    if args.palette:
        base_palette, shared_transparent0 = load_shared_palette(args.palette)
        if shared_transparent0:
            base_palette = base_palette[1:]
        visible_palette = base_palette[:visible_slots]
        if not visible_palette:
            raise SystemExit("ERROR: shared palette has no visible colors")
    else:
        visible_palette = quantize_amt_palette(opaque_pixels, visible_slots)

    # Shared palettes are mapped through the measured AMT calibration and deduplicated.
    # Auto-quantized palettes are already calibrated AMT codes, but running through
    # this path keeps both modes consistent.
    visible_amt = []
    visible_palette_snapped: List[RGB] = []
    for color in visible_palette:
        amt = rgb_to_amt(color)
        if amt in visible_amt:
            continue
        visible_amt.append(amt)
        visible_palette_snapped.append(amt_to_rgb(amt))
        if len(visible_amt) >= visible_slots:
            break
    visible_palette = visible_palette_snapped

    if not visible_palette:
        raise SystemExit("ERROR: final AMT palette contains no visible colors")

    if transparent0:
        palette_rgb: List[RGB] = [(0, 0, 0)] + visible_palette
        palette_amt: List[int] = [0] + visible_amt
        visible_offset = 1
    else:
        palette_rgb = visible_palette
        palette_amt = visible_amt
        visible_offset = 0

    # Keep at most 16 entries, but the generated header always contains 16.
    palette_rgb = palette_rgb[:16]
    palette_amt = palette_amt[:16]

    indexes: List[int] = []
    for r, g, b, a in src:
        if transparent0 and a < 128:
            indexes.append(0)
        else:
            local = nearest_amt_index((r, g, b), visible_palette)
            indexes.append(local + visible_offset)

    tile_w, tile_h = 16, 22
    cols = math.ceil(width / tile_w)
    rows = math.ceil(height / tile_h)
    padded_w = cols * tile_w
    padded_h = rows * tile_h
    pad_index = 0

    # Pad in screen coordinates first.
    padded = [pad_index] * (padded_w * padded_h)
    for y in range(height):
        src_off = y * width
        dst_off = y * padded_w
        padded[dst_off:dst_off + width] = indexes[src_off:src_off + width]

    words_per_row = math.ceil(tile_w / 4)
    words: List[int] = []

    # Tile-major, row-major inside each tile. Hardware testing confirmed that
    # the leftmost 4bpp pixel occupies the HIGH nibble of each 16-bit word:
    # P0<<12 | P1<<8 | P2<<4 | P3.
    for ty in range(rows):
        for tx in range(cols):
            for py in range(tile_h):
                y = ty * tile_h + py
                for wx in range(words_per_row):
                    x0 = tx * tile_w + wx * 4
                    p = []
                    for k in range(4):
                        x = x0 + k
                        p.append(padded[y * padded_w + x] if x < padded_w else pad_index)
                    word = ((p[0] & 0xF) << 12) | ((p[1] & 0xF) << 8) | ((p[2] & 0xF) << 4) | (p[3] & 0xF)
                    words.append(word)

    byte_count = len(words) * 2
    if byte_count > args.max_bytes:
        print(f"ERROR: bitmap requires {byte_count} bytes; --max-bytes is {args.max_bytes}", file=sys.stderr)
        return 1

    symbol = sanitize_name(args.name or args.output.stem)
    write_header(args.output, symbol, width, height, tile_w, tile_h, cols, rows,
                 palette_amt, transparent0, words)

    if args.export_palette:
        export_palette(args.export_palette, palette_rgb, transparent0)

    if args.preview:
        preview = Image.new("RGBA", (width, height))
        out = []
        for idx in indexes:
            if transparent0 and idx == 0:
                out.append((0, 0, 0, 0))
            else:
                r, g, b = palette_rgb[idx]
                out.append((r, g, b, 255))
        preview.putdata(out)
        args.preview.parent.mkdir(parents=True, exist_ok=True)
        # Explicitly replace an existing preview so repeated conversions never
        # leave a stale image behind. PIL normally overwrites files, but doing
        # this explicitly makes the converter behavior deterministic.
        if args.preview.exists():
            args.preview.unlink()
        preview.save(args.preview)

    original_colors = len(set(src))
    print(f"Input: {args.input}")
    print(f"Size: {width} x {height}")
    print(f"Original RGBA colors: {original_colors}")
    print(f"AMT palette entries used: {len(palette_rgb)} ({'palette 0 transparent' if transparent0 else 'opaque'})")
    print("Final AMT palette (index: BGR12 -> calibrated display preview):")
    for i, value in enumerate(palette_amt):
        r, g, b = amt_to_rgb(value)
        print(f"  {i:X}: 0x{value:03X} -> #{r:02X}{g:02X}{b:02X}")
    print(f"Tile grid: {cols} x {rows} ({tile_w} x {tile_h} pixels per tile)")
    print(f"Padded hardware area: {padded_w} x {padded_h}")
    print(f"Font RAM storage: {len(words)} words / {byte_count} bytes")
    print(f"Header: {args.output}")
    if args.preview:
        print(f"Preview: {args.preview}")
    if args.export_palette:
        print(f"Palette JSON: {args.export_palette}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())