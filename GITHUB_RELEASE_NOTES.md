# AMT630A OSD 0.7.0

Version 0.7.0 is the first release packaged for public distribution through GitHub and the Arduino Library Manager.

## Highlights

- Complete GitHub/Arduino documentation and API reference.
- Full documentation for the bundled Python bitmap converter.
- New `LargeFontCharacterSetBrowser` example.
- New `SmallFontCharacterSetBrowser` example using the verified SmallFont ROM range.
- Updated `LargeFontHUD` five-window example.
- Updated `SmallFontHUD` five-window example.
- Updated `EnterFactoryMode` example.
- Arduino Library Manager metadata and Arduino Lint GitHub Actions workflow.

## Existing capabilities

Five OSD windows; LargeFont and SmallFont text; custom LargeFont glyphs; 4bpp bitmaps; shared 16-entry bitmap palette; palette animation; opacity and video blending; 1x-4x window scaling; blink regions; window movement; Index RAM and Font RAM accounting; and optional factory-menu keypad automation for compatible monitor firmware.

## Important hardware note

On the tested stock firmware, repeated external I2C ownership of AMT630A internal registers can eventually freeze the complete display pipeline. Long-duration testing found that entering the monitor's hidden factory menu before sustained OSD access prevented the observed contention/freeze. The factory-menu sequence is firmware-specific; see the README and `EnterFactoryMode` example.

See `CHANGELOG.md` and `README.md` for full details.