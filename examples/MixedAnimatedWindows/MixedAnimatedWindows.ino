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

/*
  AMT630A_OSD v0.5.8 - MixedAnimatedWindows
  ==================================

  Mixed animation test using all five AMT630A OSD windows:

    Window 0 : animated text window
    Window 1 : animated text window
    Window 2 : animated robot bitmap
    Window 3 : animated robot bitmap
    Window 4 : animated robot bitmap

  This is based on AnimatedWindows and AnimatedBitmaps.

  Important:
    - LargeFont mode is used globally.
    - robot.h is loaded ONCE into Font RAM.
    - All three bitmap windows reuse the same BitmapHandle.
    - Text windows continue to use the internal character ROM.
    - Each animation frame uses ONE SHORT batch containing only the five
      moveWindow() calls.
    - Less-frequent text/color Index RAM changes use separate short batches.
    - Windows remain visible while moving; no hide/show flicker.

  Put robot.h in the same Arduino sketch folder.

  Serial Monitor: 115200 baud
*/

static const uint8_t TEXT_COUNT   = 2;
static const uint8_t ROBOT_COUNT  = 3;
static const uint8_t WINDOW_COUNT = 5;

static const uint8_t TEXT_COLS = 5;
static const uint8_t TEXT_ROWS = 5;
static const uint16_t TEXT_W = TEXT_COLS * 16;  // LargeFont cell width
static const uint16_t TEXT_H = TEXT_ROWS * 22;  // LargeFont cell height

static const uint32_t FRAME_MS        = 60;
static const uint32_t TEXT_CHANGE_MS  = 500;
static const uint32_t COLOR_CHANGE_MS = 700;

struct MovingObject {
  int16_t x, y;
  int8_t dx, dy;
};

// Windows 0 and 1 are text.
// Windows 2, 3 and 4 are robots.
MovingObject objects[WINDOW_COUNT] = {
  {  20,  20,  2,  2 },   // W0 text
  { 120,  45,  3,  2 },   // W1 text
  { 220,  70, -2,  3 },   // W2 robot
  { 320,  95, -3,  2 },   // W3 robot
  { 410, 120, -2, -3 }    // W4 robot
};

static const AMT630A_OSD::Color textBaseColor[TEXT_COUNT] = {
  AMT630A_OSD::Red,
  AMT630A_OSD::Green
};

uint32_t lastFrame = 0;
uint32_t lastTextChange = 0;
uint32_t lastColorChange = 0;

char window0Char = '0';
bool window1Blue = false;
bool running = false;

void printError(const char *where)
{
  Serial.print(where);
  Serial.print(": ");
  Serial.println(AMT630A_OSD::errorToString(display.getLastError()));
}

void bounceObject(uint8_t windowNumber, uint16_t objectW, uint16_t objectH)
{
  MovingObject &o = objects[windowNumber];

  o.x += o.dx;
  o.y += o.dy;

  int16_t maxX =
      (int16_t)display.getDisplayWidth() - (int16_t)objectW;
  int16_t maxY =
      (int16_t)display.getDisplayHeight() - (int16_t)objectH;

  if (maxX < 0) maxX = 0;
  if (maxY < 0) maxY = 0;

  if (o.x <= 0) {
    o.x = 0;
    o.dx = abs(o.dx);
  }
  else if (o.x >= maxX) {
    o.x = maxX;
    o.dx = -abs(o.dx);
  }

  if (o.y <= 0) {
    o.y = 0;
    o.dy = abs(o.dy);
  }
  else if (o.y >= maxY) {
    o.y = maxY;
    o.dy = -abs(o.dy);
  }
}

bool setupMixedWindows()
{
  // Both text and bitmap windows use LargeFont geometry.
  if (!display.setFontMode(AMT630A_OSD::LargeFont)) {
    printError("setFontMode");
    return false;
  }

  /*
    Text uses the AMT630A's internal character ROM, so we can give all
    available Font RAM bitmap storage to robot.h.
  */
  if (!display.configureFontRAM(0)) {
    printError("configureFontRAM");
    return false;
  }

  // Load robot.h only once. Windows 2, 3 and 4 share this resource.
  if (!display.loadBitmap(robot, robotHandle)) {
    printError("loadBitmap");
    return false;
  }

  const uint16_t robotW = display.getBitmapWidth(robotHandle);
  const uint16_t robotH = display.getBitmapHeight(robotHandle);

  Serial.print("Robot bitmap: ");
  Serial.print(robotW);
  Serial.print(" x ");
  Serial.println(robotH);

  /*
    Group the five window configurations into one bounded setup batch.
    Animation itself will use much shorter batches containing only movement.
  */
  if (!display.beginBatchUpdate()) {
    printError("beginBatchUpdate setup");
    return false;
  }

  bool ok = true;

  // -------------------------------------------------------------------------
  // Windows 0 and 1: text
  // -------------------------------------------------------------------------
  for (uint8_t i = 0; i < TEXT_COUNT && ok; ++i) {
    ok = display.configureWindow(i, TEXT_W, TEXT_H);

    if (ok)
      ok = display.setDisplayOrigin(i, objects[i].x, objects[i].y);

    if (ok)
      ok = display.fillRect(i, 0, 0,
                            TEXT_COLS, TEXT_ROWS,
                            textBaseColor[i]);

    char label[2] = { char('0' + i), '\0' };

    if (ok)
      ok = display.writeString(i, 0, 0, label,
                               AMT630A_OSD::White,
                               textBaseColor[i]) == 1;

    if (ok)
      ok = display.setVisible(i, true);
  }

  // -------------------------------------------------------------------------
  // Windows 2, 3 and 4: robot bitmap
  // -------------------------------------------------------------------------
  for (uint8_t i = TEXT_COUNT; i < WINDOW_COUNT && ok; ++i) {
    ok = display.configureBitmapWindow(i, robotHandle);

    if (ok)
      ok = display.setDisplayOrigin(i, objects[i].x, objects[i].y);

    if (ok)
      ok = display.setVisible(i, true);
  }

  /*
    Push the text Index RAM and the three bitmap-window Index RAM blocks once.
    During movement, only window position registers change.
  */
  if (ok)
    ok = display.forceUpdate();

  const bool closed = display.endBatchUpdate();

  if (!ok) {
    printError("mixed window setup");
    return false;
  }

  if (!closed) {
    printError("endBatchUpdate setup");
    return false;
  }

  return true;
}

bool moveAllObjects()
{
  const uint16_t robotW = display.getBitmapWidth(robotHandle);
  const uint16_t robotH = display.getBitmapHeight(robotHandle);

  // Update positions in RAM first.
  bounceObject(0, TEXT_W, TEXT_H);
  bounceObject(1, TEXT_W, TEXT_H);

  bounceObject(2, robotW, robotH);
  bounceObject(3, robotW, robotH);
  bounceObject(4, robotW, robotH);

  /*
    VERIFIED stable animation pattern from AnimatedWindows/AnimatedBitmaps:
    one short C6-open interval for all related position updates.
  */
  if (!display.beginBatchUpdate()) {
    printError("beginBatchUpdate movement");
    return false;
  }

  bool ok = true;

  for (uint8_t i = 0; i < WINDOW_COUNT && ok; ++i)
    ok = display.moveWindow(i, objects[i].x, objects[i].y);

  const bool closed = display.endBatchUpdate();

  if (!ok) {
    printError("moveWindow");
    return false;
  }

  if (!closed) {
    printError("endBatchUpdate movement");
    return false;
  }

  return true;
}

bool updateWindow0Text()
{
  if (++window0Char > '9')
    window0Char = '0';

  char label[2] = { window0Char, '\0' };

  if (!display.beginBatchUpdate())
    return false;

  bool ok =
      display.writeString(0, 0, 0, label,
                          AMT630A_OSD::White,
                          AMT630A_OSD::Red) == 1;

  if (ok)
    ok = display.forceUpdate(0);

  const bool closed = display.endBatchUpdate();

  return ok && closed;
}

bool updateWindow1Color()
{
  window1Blue = !window1Blue;

  AMT630A_OSD::Color bg =
      window1Blue ? AMT630A_OSD::Blue
                  : AMT630A_OSD::Green;

  if (!display.beginBatchUpdate())
    return false;

  bool ok =
      display.fillRect(1, 0, 0,
                       TEXT_COLS, TEXT_ROWS, bg);

  char label[2] = { '1', '\0' };

  if (ok)
    ok = display.writeString(1, 0, 0, label,
                             AMT630A_OSD::White, bg) == 1;

  if (ok)
    ok = display.forceUpdate(1);

  const bool closed = display.endBatchUpdate();

  return ok && closed;
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("AMT630A_OSD v0.5.8 - MixedAnimatedWindows");
  Serial.println("=================================");
  Serial.println("2 animated text windows + 3 animated robot bitmaps");

  if (!display.begin()) {
    printError("begin");
    return;
  }

  if (!setupMixedWindows())
    return;

  Serial.println();
  Serial.print("Display: ");
  Serial.print(display.getDisplayWidth());
  Serial.print(" x ");
  Serial.println(display.getDisplayHeight());

  Serial.print("Index RAM used: ");
  Serial.println(display.getIndexRAMUsed());

  Serial.print("Index RAM free: ");
  Serial.println(display.getIndexRAMFree());

  Serial.println();
  Serial.println("W0/W1 = moving text");
  Serial.println("W2/W3/W4 = moving copies of robot.h");
  Serial.println("All five move in the same short animation batch.");

  const uint32_t now = millis();
  lastFrame = now;
  lastTextChange = now;
  lastColorChange = now;

  running = true;
}

void loop()
{
  if (!running)
    return;

  const uint32_t now = millis();

  if ((uint32_t)(now - lastFrame) >= FRAME_MS) {
    lastFrame = now;

    if (!moveAllObjects()) {
      printError("moveAllObjects");
      running = false;
      return;
    }
  }

  if ((uint32_t)(now - lastTextChange) >= TEXT_CHANGE_MS) {
    lastTextChange = now;

    if (!updateWindow0Text()) {
      printError("updateWindow0Text");
      running = false;
      return;
    }
  }

  if ((uint32_t)(now - lastColorChange) >= COLOR_CHANGE_MS) {
    lastColorChange = now;

    if (!updateWindow1Color()) {
      printError("updateWindow1Color");
      running = false;
      return;
    }
  }
}