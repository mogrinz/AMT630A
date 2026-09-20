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
#include "robot.h"

AMT630A_OSD display;
AMT630A_OSD::BitmapHandle robotHandle;

// ------------------------------------------------------------
// Movement tuning
// ------------------------------------------------------------

static const uint32_t MOVE_INTERVAL_MS = 30;
static const uint16_t MOVE_STEP_PIXELS = 1;

// Hardware-tested positioning adjustment for this display.
// Start the robot 20 pixels in from the left, and allow the
// OSD travel range to extend 20 pixels past the nominal width.
static const uint16_t LEFT_EDGE_OFFSET_PIXELS  = 20;
static const uint16_t RIGHT_EDGE_EXTRA_PIXELS = 20;

uint16_t robotX = LEFT_EDGE_OFFSET_PIXELS;
uint16_t robotY = 0;
int8_t direction = 1;
uint32_t lastMoveMs = 0;

// ------------------------------------------------------------
// Palette animation tuning
// ------------------------------------------------------------

// Palette index 5 is the yellow chest light in the bundled robot.h.
// robot_palette[5] is 0x6DF (12-bit BGR), which decodes to RGB 0xFF,0xDD,0x66.
static const uint8_t CHEST_PALETTE_INDEX = 5;

// AMT630A bitmap colors are stored as 12-bit BGR: 0xBGR.
// 0x00F = full red.
static const uint16_t CHEST_RED = 0x00F;

// Leave the red state on long enough to be clearly visible.
static const uint32_t CHEST_YELLOW_TIME_MS = 900;
static const uint32_t CHEST_RED_TIME_MS    = 400;

uint16_t chestNormalColor = 0;
bool chestIsRed = false;
uint32_t lastChestChangeMs = 0;

// ------------------------------------------------------------
// Error helper
// ------------------------------------------------------------

void printError(const char *where)
{
  Serial.print(where);
  Serial.print(": ");
  Serial.println(AMT630A_OSD::errorToString(display.getLastError()));
}

// ------------------------------------------------------------

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("AMT630A ANIMATION + PALETTE DEMO 1");
  Serial.println("---------------------------------");

  if (!display.begin()) {
    printError("begin");
    return;
  }

  // Bitmap mode currently uses the verified 16x22 LargeFont geometry.
  if (!display.setFontMode(AMT630A_OSD::LargeFont)) {
    printError("setFontMode");
    return;
  }

  // Give the entire Font RAM bitmap area to bitmap resources.
  if (!display.configureFontRAM(0)) {
    printError("configureFontRAM");
    return;
  }

  // robot.h is bundled with this example.
  if (!display.loadBitmap(robot, robotHandle)) {
    printError("loadBitmap");
    return;
  }

  // Save the robot's original chest color from its loaded palette.
  // This avoids hard-coding the yellow value from robot.h.
  chestNormalColor =
      display.getBitmapPaletteColorRaw(CHEST_PALETTE_INDEX);

  if (!display.configureBitmapWindow(0, robotHandle)) {
    printError("configureBitmapWindow");
    return;
  }

  // Place the logical bitmap against the bottom of the configured display.
  const uint16_t displayHeight = display.getDisplayHeight();
  const uint16_t robotHeight = display.getBitmapHeight(robotHandle);
  robotY = (robotHeight < displayHeight) ? (displayHeight - robotHeight) : 0;

  robotX = LEFT_EDGE_OFFSET_PIXELS;

  // setDisplayOrigin() is used only for the one-time initial setup.
  if (!display.setDisplayOrigin(0, robotX, robotY)) {
    printError("setDisplayOrigin");
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

  Serial.println("Bitmap loaded successfully.");
  Serial.print("Display: ");
  Serial.print(display.getDisplayWidth());
  Serial.print(" x ");
  Serial.println(display.getDisplayHeight());

  Serial.print("Robot: ");
  Serial.print(display.getBitmapWidth(robotHandle));
  Serial.print(" x ");
  Serial.println(display.getBitmapHeight(robotHandle));

  Serial.print("Bottom Y: ");
  Serial.println(robotY);

  Serial.print("Chest palette index: ");
  Serial.println(CHEST_PALETTE_INDEX);

  Serial.print("Original chest color: 0x");
  Serial.println(chestNormalColor, HEX);
}

// ------------------------------------------------------------

void loop()
{
  // Keep the library's normal visibility/coexistence maintenance running.
  display.service();

  const uint32_t now = millis();

  // ----------------------------------------------------------
  // Move the robot left and right.
  // ----------------------------------------------------------

  if ((uint32_t)(now - lastMoveMs) >= MOVE_INTERVAL_MS) {
    lastMoveMs = now;

    const uint16_t displayWidth =
        display.getDisplayWidth() + RIGHT_EDGE_EXTRA_PIXELS;
    const uint16_t robotWidth = display.getBitmapWidth(robotHandle);
    const uint16_t rightEdge =
        (robotWidth < displayWidth) ? (displayWidth - robotWidth) : 0;

    if (direction > 0) {
      if (robotX + MOVE_STEP_PIXELS >= rightEdge) {
        robotX = rightEdge;
        direction = -1;
      } else {
        robotX += MOVE_STEP_PIXELS;
      }
    } else {
      if (robotX <= LEFT_EDGE_OFFSET_PIXELS + MOVE_STEP_PIXELS) {
        robotX = LEFT_EDGE_OFFSET_PIXELS;
        direction = 1;
      } else {
        robotX -= MOVE_STEP_PIXELS;
      }
    }

    // moveWindow() performs the lightweight position-only update intended
    // for animation. Do not use setDisplayOrigin() for every frame.
    if (!display.moveWindow(0, robotX, robotY)) {
      printError("moveWindow");
    }
  }

  // ----------------------------------------------------------
  // Blink the yellow chest light red, then return to yellow.
  // Only palette entry 6 changes; bitmap data is untouched.
  // ----------------------------------------------------------

  const uint32_t chestInterval =
      chestIsRed ? CHEST_RED_TIME_MS : CHEST_YELLOW_TIME_MS;

  if ((uint32_t)(now - lastChestChangeMs) >= chestInterval) {
    lastChestChangeMs = now;
    chestIsRed = !chestIsRed;

    const uint16_t newColor =
        chestIsRed ? CHEST_RED : chestNormalColor;

    if (!display.setBitmapPaletteColorRaw(
          CHEST_PALETTE_INDEX,
          newColor)) {
      printError("setBitmapPaletteColorRaw");
    }
  }
}