# Changelog

## 0.7.0

This is the first release packaged as a complete public GitHub / Arduino Library Manager distribution.

### Added
- `LargeFontCharacterSetBrowser` example for browsing raw LargeFont glyph IDs from the Serial Monitor.
- `SmallFontCharacterSetBrowser` example, starting at the verified SmallFont ROM range (`0x1C0`).
- Complete GitHub-facing library documentation and API reference.
- Complete documentation for `tools/bitmap_converter/amt630a_bitmap_converter.py`.
- Arduino Library Manager publication metadata and Arduino Lint GitHub Actions workflow.
- Release and Library Manager publication checklist.

### Updated
- `LargeFontHUD` now demonstrates a five-window Robby-style HUD layout with READY, POWER, and six status fields.
- `SmallFontHUD` now demonstrates the equivalent five-window SmallFont HUD layout.
- `EnterFactoryMode` now uses the hardware-tested SmallFont blue status window and current factory-menu sequence/timing.

### Retained from 0.6.3
- Optional GPIO/74HC4066 factory-menu automation.
- Five OSD windows, LargeFont/SmallFont text, custom glyphs, 4bpp bitmaps, palette animation, scaling, opacity/blending, blink regions, window movement, batching, and memory inspection helpers.
- Released under the MIT License.
