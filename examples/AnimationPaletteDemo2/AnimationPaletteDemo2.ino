/*
Library Name: AMT630A On-Screen-Display (OSD)
Created by: Michael Ogrinz, September 2026
Contact: mike@ogrinz.com, https://youtube.com/@ogrinz_labs
Description: A library for creating on-screen text and bitmap graphics over a live composite video signal for devices using the AMT630A video decoder and digital TFT-LCD panel-control SoC.
Repository: https://github.com/mogrinz/AMT630A
License: MIT License
Changelog:
v0.7.0 - 2026-09-20: Initial release.
*/

#include <AMT630A_OSD.h>
#include "radar.h"

/*
  AnimationPaletteDemo2
  =====================

  Demonstrates palette-based bitmap animation.

  A radar image is loaded once into bitmap memory. The image uses:
    Palette 0     - transparent area outside the radar
    Palette 1     - permanent white radar markings
    Palettes 2-15 - individual radar sweep slices

  Animation is performed entirely by changing palette colors:
    - the previous slice is changed to black
    - the next slice is changed to green

  The bitmap data itself is never rewritten.

  Alpha blending is enabled with opacity 4, allowing the underlying
  video to remain visible through the black radar background.

  This technique provides very fast animation because only two
  palette entries need to be changed for each frame.
*/

AMT630A_OSD display;

AMT630A_OSD::BitmapHandle radarHandle =
    AMT630A_OSD::INVALID_BITMAP_HANDLE;

// Clockwise order of the 14 radar-slice palette entries.
static const uint8_t sweepOrder[14] = {
  15, 6, 2, 3, 8, 10, 11,
  14, 5, 4, 7, 9, 12, 13
};

static const uint16_t STEP_MS = 150;

uint8_t currentSlice = 0;
uint32_t lastStep = 0;

void printError(const char *where)
{
  Serial.print(where);
  Serial.print(": ");
  Serial.println(
    AMT630A_OSD::errorToString(
      display.getLastError()
    )
  );
}

bool setSliceBlack(uint8_t paletteIndex)
{
  return display.setBitmapPaletteColor(
    paletteIndex,
    0, 0, 0
  );
}

bool setSliceGreen(uint8_t paletteIndex)
{
  return display.setBitmapPaletteColor(
    paletteIndex,
    0, 255, 0
  );
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("AMT630A Animation Palette Demo 2");
  Serial.println("================================");

  if (!display.begin()) {
    printError("begin");
    return;
  }

  if (!display.setFontMode(AMT630A_OSD::LargeFont)) {
    printError("setFontMode");
    return;
  }

  // Give all Font RAM to bitmap resources.
  if (!display.configureFontRAM(0)) {
    printError("configureFontRAM");
    return;
  }

  if (!display.loadBitmap(radar, radarHandle)) {
    printError("loadBitmap");
    return;
  }

  if (!display.configureBitmapWindow(0, radarHandle)) {
    printError("configureBitmapWindow");
    return;
  }

  if (!display.setDisplayOrigin(0, 100, 60)) {
    printError("setDisplayOrigin");
    return;
  }

  // Blend the radar over the underlying composite video.
  if (!display.setBlendingEnabled(true)) {
    printError("setBlendingEnabled");
    return;
  }

  if (!display.setOpacity(4)) {
    printError("setOpacity");
    return;
  }

  if (!display.beginBatchUpdate()) {
    printError("beginBatchUpdate");
    return;
  }

  bool ok = true;

  // Palette 0 is used only outside the circular radar image.
  if (ok)
    ok = display.setBitmapPalette0Transparent(true);

  // Palette 1 contains the fixed white grid/markings.
  if (ok)
    ok = display.setBitmapPaletteColor(1, 255, 255, 255);

  // All radar wedges start black except the active green wedge.
  for (uint8_t i = 2; ok && i < 16; ++i)
    ok = setSliceBlack(i);

  if (ok)
    ok = setSliceGreen(sweepOrder[currentSlice]);

  bool batchOK = display.endBatchUpdate();

  if (!ok || !batchOK) {
    printError("palette setup");
    return;
  }

  if (!display.setVisible(0, true)) {
    printError("setVisible");
    return;
  }

  if (!display.forceUpdate()) {
    printError("forceUpdate");
    return;
  }

  lastStep = millis();
}

void loop()
{
  // Required so the library can maintain the AMT630A OSD state.
  display.service();

  uint32_t now = millis();

  if ((uint32_t)(now - lastStep) < STEP_MS)
    return;

  lastStep = now;

  uint8_t oldPaletteIndex = sweepOrder[currentSlice];

  ++currentSlice;
  if (currentSlice >= 14)
    currentSlice = 0;

  uint8_t newPaletteIndex = sweepOrder[currentSlice];

  if (!display.beginBatchUpdate()) {
    printError("beginBatchUpdate animation");
    return;
  }

  bool ok =
      setSliceBlack(oldPaletteIndex) &&
      setSliceGreen(newPaletteIndex);

  bool batchOK = display.endBatchUpdate();

  if (!ok || !batchOK)
    printError("palette animation");
}