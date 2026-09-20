#include <AMT630A_OSD.h>
#include "robot.h"

AMT630A_OSD display;
AMT630A_OSD::BitmapHandle robotHandle;

/*
  AMT630A_OSD - AnimatedBitmaps
  ============================

  Five copies of the SAME 64x64 robot bitmap move continuously using
  OSD windows 0..4.

  This is intentionally based on AnimatedWindows so we can compare tearing
  between moving text windows and moving bitmap windows under nearly the same
  conditions.

  Important:
    - robot.h is loaded ONCE into Font RAM.
    - All five bitmap windows reuse the same BitmapHandle.
    - Each animation frame uses ONE SHORT batch containing only the five
      moveWindow() calls.
    - There are no repeated bitmap-data writes during animation.
    - Windows remain visible while moving; no hide/show flicker.

  This makes it a useful test of the theory that the AMT630A may internally
  synchronize bitmap-window display updates differently from normal text
  windows.

  Put your generated 64x64 robot.h in the same Arduino sketch folder.

  Serial Monitor: 115200 baud
*/

static const uint8_t BITMAP_COUNT = 5;
static const uint32_t FRAME_MS = 60;

struct MovingBitmap {
  int16_t x;
  int16_t y;
  int8_t dx;
  int8_t dy;
};

MovingBitmap robots[BITMAP_COUNT] = {
  {  20,  20,  2,  2 },
  { 110,  40,  3,  2 },
  { 200,  60, -2,  3 },
  { 290,  80, -3,  2 },
  { 380, 100, -2, -3 }
};

uint32_t lastFrame = 0;
bool running = false;

void printError(const char *where)
{
  Serial.print(where);
  Serial.print(": ");
  Serial.println(AMT630A_OSD::errorToString(display.getLastError()));
}

void bounceBitmap(uint8_t i, uint16_t bitmapW, uint16_t bitmapH)
{
  MovingBitmap &r = robots[i];

  r.x += r.dx;
  r.y += r.dy;

  int16_t maxX = (int16_t)display.getDisplayWidth()  - (int16_t)bitmapW;
  int16_t maxY = (int16_t)display.getDisplayHeight() - (int16_t)bitmapH;

  if (maxX < 0) maxX = 0;
  if (maxY < 0) maxY = 0;

  if (r.x <= 0) {
    r.x = 0;
    r.dx = abs(r.dx);
  }
  else if (r.x >= maxX) {
    r.x = maxX;
    r.dx = -abs(r.dx);
  }

  if (r.y <= 0) {
    r.y = 0;
    r.dy = abs(r.dy);
  }
  else if (r.y >= maxY) {
    r.y = maxY;
    r.dy = -abs(r.dy);
  }
}

bool setupBitmaps()
{
  // Bitmap mode uses the verified LargeFont 16x22 tile geometry.
  if (!display.setFontMode(AMT630A_OSD::LargeFont)) {
    printError("setFontMode");
    return false;
  }

  // Give all available Font RAM bitmap storage to bitmap resources.
  if (!display.configureFontRAM(0)) {
    printError("configureFontRAM");
    return false;
  }

  // Load robot.h ONCE. All five windows will reference this same resource.
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

  Serial.println("Configuring five bitmap windows...");

  /*
    Keep setup grouped so the related window-register writes do not create
    many independent C6 unlock/relock cycles. This setup is small: each
    bitmap window uses only the bitmap's tile grid in Index RAM.
  */
  if (!display.beginBatchUpdate()) {
    printError("beginBatchUpdate setup");
    return false;
  }

  bool ok = true;

  for (uint8_t i = 0; i < BITMAP_COUNT && ok; ++i) {
    ok = display.configureBitmapWindow(i, robotHandle);

    if (ok)
      ok = display.setDisplayOrigin(i, robots[i].x, robots[i].y);

    if (ok)
      ok = display.setVisible(i, true);
  }

  /*
    Write the bitmap-window Index RAM once. During animation the bitmap
    contents do not change; only X/Y position registers are updated.
  */
  if (ok)
    ok = display.forceUpdate();

  const bool closed = display.endBatchUpdate();

  if (!ok) {
    printError("bitmap window setup");
    return false;
  }

  if (!closed) {
    printError("endBatchUpdate setup");
    return false;
  }

  return true;
}

bool moveAllBitmaps()
{
  const uint16_t robotW = display.getBitmapWidth(robotHandle);
  const uint16_t robotH = display.getBitmapHeight(robotHandle);

  for (uint8_t i = 0; i < BITMAP_COUNT; ++i)
    bounceBitmap(i, robotW, robotH);

  /*
    Same short-batch approach that proved stable in AnimatedWindows:
    one C6-open interval for the five related position updates.
  */
  if (!display.beginBatchUpdate()) {
    printError("beginBatchUpdate movement");
    return false;
  }

  bool ok = true;

  for (uint8_t i = 0; i < BITMAP_COUNT && ok; ++i)
    ok = display.moveWindow(i, robots[i].x, robots[i].y);

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

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("AMT630A_OSD - AnimatedBitmaps");
  Serial.println("============================");

  if (!display.begin()) {
    printError("begin");
    return;
  }

  if (!setupBitmaps())
    return;

  lastFrame = millis();
  running = true;

  Serial.println();
  Serial.println("Five robot bitmaps are now moving.");
  Serial.println("Watch specifically for tearing, corruption,");
  Serial.println("disappearing windows, or AMT630A lock-up.");
}

void loop()
{
  if (!running)
    return;

  const uint32_t now = millis();

  if ((uint32_t)(now - lastFrame) < FRAME_MS)
    return;

  lastFrame = now;

  if (!moveAllBitmaps()) {
    Serial.println("Animation stopped.");
    running = false;
  }
}