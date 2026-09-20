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
#include "bitmap1.h"
#include "bitmap2.h"

/*
  HousePaletteBitmaps
  ===================

  Demonstrates:
    - Two independently loaded bitmap resources
    - A shared AMT630A House Palette
    - 3x built-in hardware scaling
    - Independent vertical motion with no overlap

  bitmap1.h and bitmap2.h should both be generated with:

      --housepalette

  IMPORTANT:
    The AMT630A bitmap palette is global, so both images must use the
    same palette. The House Palette guarantees that separately converted
    images use identical palette indexes.

  Display assumptions:
    480 x 272 panel
    Each 24x24 source image occupies a 2x2 LargeFont bitmap tile area:
       32 x 44 pixels before scaling
       96 x 132 pixels at 3x scale

  Motion:
    - bitmap1 moves up/down on the left side
    - bitmap2 moves up/down on the right side
    - X positions are fixed so the scaled windows never overlap
*/

AMT630A_OSD display;

AMT630A_OSD::BitmapHandle bitmap1Handle =
    AMT630A_OSD::INVALID_BITMAP_HANDLE;

AMT630A_OSD::BitmapHandle bitmap2Handle =
    AMT630A_OSD::INVALID_BITMAP_HANDLE;


// -----------------------------------------------------------------------------
// Display / animation geometry
// -----------------------------------------------------------------------------

static const int16_t SCREEN_WIDTH  = 480;
static const int16_t SCREEN_HEIGHT = 272;

static const uint8_t BITMAP_SCALE = 3;

// 24x24 source -> 2x2 bitmap tiles.
// LargeFont bitmap tile = 16x22.
static const int16_t BITMAP_WINDOW_WIDTH  = 32 * BITMAP_SCALE;
static const int16_t BITMAP_WINDOW_HEIGHT = 44 * BITMAP_SCALE;

static const int16_t MAX_Y =
    SCREEN_HEIGHT - BITMAP_WINDOW_HEIGHT;

// Fixed horizontal positions with a large safety gap.
static const int16_t BITMAP1_X = 55;
static const int16_t BITMAP2_X = 325;


// -----------------------------------------------------------------------------
// Motion state
// -----------------------------------------------------------------------------

struct VerticalMover
{
  uint8_t window;
  int16_t x;
  int16_t y;
  int8_t direction;   // +1 = down, -1 = up
};

VerticalMover mover1 = { 0, BITMAP1_X, 10, +1 };
VerticalMover mover2 = { 1, BITMAP2_X, MAX_Y - 10, -1 };

static const uint16_t FRAME_INTERVAL_MS = 25;
static const int16_t MOVE_STEP = 2;

uint32_t lastFrameTime = 0;


// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

void printError(const char *where)
{
  Serial.print(where);
  Serial.print(": ");
  Serial.println(
      AMT630A_OSD::errorToString(display.getLastError()));
}


void advanceMover(VerticalMover &m)
{
  m.y += (int16_t)m.direction * MOVE_STEP;

  if (m.y <= 0)
  {
    m.y = 0;
    m.direction = +1;
  }
  else if (m.y >= MAX_Y)
  {
    m.y = MAX_Y;
    m.direction = -1;
  }
}


bool updatePositions()
{
  if (!display.beginBatchUpdate())
  {
    printError("beginBatchUpdate");
    return false;
  }

  bool ok = true;

  if (!display.setDisplayOrigin(
          mover1.window,
          mover1.x,
          mover1.y))
  {
    printError("setDisplayOrigin W0");
    ok = false;
  }

  if (ok &&
      !display.setDisplayOrigin(
          mover2.window,
          mover2.x,
          mover2.y))
  {
    printError("setDisplayOrigin W1");
    ok = false;
  }

  if (!display.endBatchUpdate())
  {
    printError("endBatchUpdate");
    ok = false;
  }

  return ok;
}


// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

bool setupBitmaps()
{
  if (!display.begin())
  {
    printError("begin");
    return false;
  }

  if (!display.setFontMode(AMT630A_OSD::LargeFont))
  {
    printError("setFontMode");
    return false;
  }

  // No custom-glyph reservation in bitmap mode.
  if (!display.configureFontRAM(0))
  {
    printError("configureFontRAM");
    return false;
  }

  if (!display.loadBitmap(bitmap1, bitmap1Handle))
  {
    printError("loadBitmap bitmap1");
    return false;
  }

  if (!display.loadBitmap(bitmap2, bitmap2Handle))
  {
    printError("loadBitmap bitmap2");
    return false;
  }

  // W0 -> bitmap1
  if (!display.configureBitmapWindow(0, bitmap1Handle))
  {
    printError("configureBitmapWindow W0");
    return false;
  }

  // W1 -> bitmap2
  if (!display.configureBitmapWindow(1, bitmap2Handle))
  {
    printError("configureBitmapWindow W1");
    return false;
  }

  // Use the AMT630A's hardware scaler.
  if (!display.setWindowScale(0, BITMAP_SCALE))
  {
    printError("setWindowScale W0");
    return false;
  }

  if (!display.setWindowScale(1, BITMAP_SCALE))
  {
    printError("setWindowScale W1");
    return false;
  }

  if (!display.setDisplayOrigin(
          0,
          mover1.x,
          mover1.y))
  {
    printError("setDisplayOrigin W0");
    return false;
  }

  if (!display.setDisplayOrigin(
          1,
          mover2.x,
          mover2.y))
  {
    printError("setDisplayOrigin W1");
    return false;
  }

  if (!display.setVisible(0, true))
  {
    printError("setVisible W0");
    return false;
  }

  if (!display.setVisible(1, true))
  {
    printError("setVisible W1");
    return false;
  }

  if (!display.forceUpdate())
  {
    printError("forceUpdate");
    return false;
  }

  return true;
}


void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("AMT630A HousePaletteBitmaps");
  Serial.println("===========================");

  if (!setupBitmaps())
  {
    Serial.println("SETUP FAILED.");
    return;
  }

  Serial.println("Two House Palette bitmaps loaded.");
  Serial.println("Both windows scaled to 3x.");
  Serial.println("Vertical non-overlapping motion started.");

  Serial.print("Bitmap RAM used: ");
  Serial.print(display.getBitmapMemoryUsedBytes());
  Serial.println(" bytes");

  Serial.print("Bitmap RAM free: ");
  Serial.print(display.getBitmapMemoryFreeBytes());
  Serial.println(" bytes");
}


// -----------------------------------------------------------------------------
// Main loop
// -----------------------------------------------------------------------------

void loop()
{
  display.service();

  uint32_t now = millis();

  if ((uint32_t)(now - lastFrameTime) < FRAME_INTERVAL_MS)
    return;

  lastFrameTime = now;

  advanceMover(mover1);
  advanceMover(mover2);

  updatePositions();
}