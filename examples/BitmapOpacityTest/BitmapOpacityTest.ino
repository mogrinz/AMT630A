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

static const uint32_t MOVE_INTERVAL_MS = 30;
static const uint16_t MOVE_STEP_PIXELS = 1;

uint16_t robotX = 0;
uint16_t robotY = 0;
int8_t direction = 1;
uint32_t lastMoveMs = 0;

void printError(const char *where)
{
  Serial.print(where);
  Serial.print(": ");
  Serial.println(AMT630A_OSD::errorToString(display.getLastError()));
}

void printHelp()
{
  Serial.println();
  Serial.println("Controls:");
  Serial.println("  0..7 = set bitmap opacity");
  Serial.println("  +    = brightness +1");
  Serial.println("  -    = brightness -1");
  Serial.println("  x    = toggle blending");
  Serial.println();
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("AMT630A MOVING BITMAP OPACITY TEST");
  Serial.println("---------------------------------");

  if (!display.begin()) {
    printError("begin");
    return;
  }

  if (!display.setFontMode(AMT630A_OSD::LargeFont)) {
    printError("setFontMode");
    return;
  }

  if (!display.configureFontRAM(0)) {
    printError("configureFontRAM");
    return;
  }

  // Copy a generated robot.h into this example directory before compiling.
  if (!display.loadBitmap(robot, robotHandle)) {
    printError("loadBitmap");
    return;
  }

  if (!display.configureBitmapWindow(0, robotHandle)) {
    printError("configureBitmapWindow");
    return;
  }

  const uint16_t displayHeight = display.getDisplayHeight();
  const uint16_t robotHeight = display.getBitmapHeight(robotHandle);
  robotY = (robotHeight < displayHeight) ? (displayHeight - robotHeight) : 0;
  robotX = 0;

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

  // Midpoint of the hardware-verified 0..7 opacity range.
  if (!display.setOpacity(4)) {
    printError("setOpacity");
    return;
  }

  if (!display.setBlendingEnabled(true)) {
    printError("setBlendingEnabled");
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
  Serial.print("Opacity: ");
  Serial.println(display.getOpacity());
  Serial.print("Brightness: ");
  Serial.println(display.getBrightness());

  printHelp();
}

void loop()
{
  display.service();

  // Serial opacity/brightness/blending controls remain live while the robot moves.
  if (Serial.available()) {
    const char c = Serial.read();

    if (c >= '0' && c <= '7') {
      if (!display.setOpacity(c - '0'))
        printError("setOpacity");
      else {
        Serial.print("Opacity = ");
        Serial.println(display.getOpacity());
      }
    }
    else if (c == '+') {
      uint8_t brightness = display.getBrightness();
      if (brightness < 31) ++brightness;
      if (!display.setBrightness(brightness))
        printError("setBrightness");
      else {
        Serial.print("Brightness = ");
        Serial.println(display.getBrightness());
      }
    }
    else if (c == '-') {
      uint8_t brightness = display.getBrightness();
      if (brightness > 0) --brightness;
      if (!display.setBrightness(brightness))
        printError("setBrightness");
      else {
        Serial.print("Brightness = ");
        Serial.println(display.getBrightness());
      }
    }
    else if (c == 'x' || c == 'X') {
      const bool enabled = !display.isBlendingEnabled();
      if (!display.setBlendingEnabled(enabled))
        printError("setBlendingEnabled");
      else {
        Serial.print("Blending = ");
        Serial.println(display.isBlendingEnabled() ? "ENABLED" : "DISABLED");
      }
    }
  }

  const uint32_t now = millis();
  if ((uint32_t)(now - lastMoveMs) < MOVE_INTERVAL_MS)
    return;

  lastMoveMs = now;

  const uint16_t displayWidth = display.getDisplayWidth();
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
    if (robotX <= MOVE_STEP_PIXELS) {
      robotX = 0;
      direction = 1;
    } else {
      robotX -= MOVE_STEP_PIXELS;
    }
  }

  if (!display.moveWindow(0, robotX, robotY))
    printError("moveWindow");
}