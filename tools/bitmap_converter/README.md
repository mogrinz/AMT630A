# Bitmap Converter

The converter is included in:

```text
tools/bitmap_converter/amt630a_bitmap_converter.py
```

It converts the **first frame** of GIF files, or PNG/JPEG/BMP images, into a C++ header defining an `AMT630A_OSD::Bitmap`.

## Requirements

- Python 3
- Pillow

Install the Python dependency with:

```bash
python -m pip install -r tools/bitmap_converter/requirements.txt
```

or:

```bash
python -m pip install Pillow
```

## Basic conversion

```bash
python tools/bitmap_converter/amt630a_bitmap_converter.py input.png output.h --name MyLogo
```

Then place `output.h` in the Arduino sketch folder and include it:

```cpp
#include "output.h"
```

The generated symbol is the value supplied to `--name`; if omitted, the converter sanitizes the output filename stem and uses that as the symbol name.

## Converter command line

```text
amt630a_bitmap_converter.py INPUT OUTPUT [options]
```

Positional arguments:

| Argument | Meaning |
| --- | --- |
| `INPUT` | GIF, PNG, JPEG, or BMP source. GIF uses the first frame. |
| `OUTPUT` | Generated Arduino/C++ `.h` file. |

Options:

| Option | Meaning |
| --- | --- |
| `--name NAME` | C/C++ bitmap symbol name. Default: sanitized output filename stem. |
| `--font large` | Target bitmap geometry. Only `large` is currently supported; it means 16x22 tiles. |
| `--max-bytes N` | Reject output if packed Font RAM data exceeds N bytes. Default: 8192. |
| `--max-colors N` | Maximum total palette entries, including transparent index 0. Range 2-16; default 16. |
| `--opaque` | If the source has no transparency, allow palette index 0 to be an opaque color instead of reserving it for transparent padding. |
| `--palette FILE.json` | Map the image to an existing shared JSON palette instead of auto-quantizing a new palette. |
| `--export-palette FILE.json` | Save the final palette as JSON for reuse by other images. |
| `--preview FILE.png` | Write a PNG preview based on the converter's measured AMT/display calibration. Existing preview files are replaced. |

## Memory sizing

The AMT630A Font RAM contains 4096 16-bit words = 8192 bytes. A bitmap uses 4 bits/pixel and is padded to complete 16x22 tiles. The converter reports:

- source dimensions,
- tile grid,
- padded hardware dimensions,
- Font RAM words,
- Font RAM bytes.

Use `--max-bytes` when some Font RAM is unavailable to the bitmap allocator:

```bash
python tools/bitmap_converter/amt630a_bitmap_converter.py robot.png robot.h \
  --name robot \
  --max-bytes 4096
```

The converter exits with an error instead of generating an oversized bitmap.

## Transparency

By default, palette index 0 is reserved for transparent source pixels and transparent tile padding. A source pixel is treated as transparent when its alpha is below 128.

For a fully opaque source, `--opaque` allows index 0 to be used as a visible color:

```bash
python tools/bitmap_converter/amt630a_bitmap_converter.py logo.png logo.h --opaque
```

If the source itself contains transparency, index 0 remains transparent even when `--opaque` is supplied.

## Color quantization and AMT display calibration

The converter does not simply quantize to ordinary 24-bit RGB and then truncate. It first works on colors representable by the AMT RGB444 palette and uses a perceptual CIE Lab distance metric. Its palette selection/refinement is designed to use the available slots where they reduce image error rather than reserving particular colors.

The converter also contains a measured transfer calibration for the tested AMT630A/LCD combination. The observed neutral brightness relationship was approximately:

| Intended brightness | AMT nibble code |
| ---: | ---: |
| 0% | 0 |
| 25% | 2.5 |
| 50% | 3.5 |
| 75% | 5.5 |
| 100% | 15 |

The calibration is interpolated independently for red, green, and blue. Palette optimization therefore works from predicted displayed appearance rather than assuming linear RGB444 output. `--preview` applies the inverse calibration to show the converter's prediction of the resulting display colors.

This calibration is based on the tested display and should not be assumed identical on every LCD panel or AMT630A product.

## Shared palettes for multiple bitmaps

Because every bitmap window shares one hardware palette, multiple images displayed simultaneously should normally be converted against one shared palette.

Create the first image and export its palette:

```bash
python tools/bitmap_converter/amt630a_bitmap_converter.py first.png first.h \
  --name first \
  --export-palette hud_palette.json
```

Map another image to that exact palette:

```bash
python tools/bitmap_converter/amt630a_bitmap_converter.py second.png second.h \
  --name second \
  --palette hud_palette.json
```

The palette JSON contains `palette0_transparent` plus a list of RGB hex colors. The converter accepts colors in `#RRGGBB` form or three-element RGB arrays when reading a palette file.

## Preview output

```bash
python tools/bitmap_converter/amt630a_bitmap_converter.py input.png output.h \
  --preview output_preview.png
```

The preview is reconstructed from the exact palette indexes and calibrated palette values used for the generated header. If the preview path already exists, it is intentionally replaced so repeated conversions do not leave a stale preview.

## Complete converter example

```bash
python tools/bitmap_converter/amt630a_bitmap_converter.py radar.gif radar.h \
  --name radar \
  --max-colors 16 \
  --max-bytes 8192 \
  --export-palette radar_palette.json \
  --preview radar_preview.png
```

## License

The BitmapConverter tool is distributed as part of AMT630A OSD under the repository's **MIT License**. See [`../../LICENSE`](../../LICENSE).