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

AMT630A_OSD display;

// AMT630A_OSD v0.5.5 - AnimatedWindows
//
// Five overlapping windows move continuously. Position updates are grouped
// into one SHORT batch per animation frame. Less-frequent Index RAM changes
// use their own short batches.
//
// Hardware testing found this pattern reliable for extended animation:
//   * avoid many rapid C6 unlock/relock cycles for related writes
//   * keep each batch short; do not hold C6 open across a very large update
//
// Hiding windows before moving them was tested and did not reduce tearing;
// it only introduced flicker, so this example leaves windows visible.

static const uint8_t WINDOW_COUNT = 5;
static const uint8_t WINDOW_COLS = 5;
static const uint8_t WINDOW_ROWS = 5;
static const uint16_t WINDOW_W = WINDOW_COLS * 16;  // LargeFont cell width
static const uint16_t WINDOW_H = WINDOW_ROWS * 22;  // LargeFont cell height

static const uint32_t FRAME_MS = 60;
static const uint32_t TEXT_CHANGE_MS = 500;
static const uint32_t COLOR_CHANGE_MS = 700;

struct MovingWindow {
  int16_t x, y;
  int8_t dx, dy;
};

MovingWindow windows[WINDOW_COUNT] = {
  {  20,  20,  2,  2 },
  { 110,  40,  3,  2 },
  { 200,  60, -2,  3 },
  { 290,  80, -3,  2 },
  { 380, 100, -2, -3 }
};

static const AMT630A_OSD::Color baseColor[WINDOW_COUNT] = {
  AMT630A_OSD::Red,
  AMT630A_OSD::Green,
  AMT630A_OSD::Blue,
  AMT630A_OSD::Cyan,
  AMT630A_OSD::Yellow
};

uint32_t lastFrame = 0;
uint32_t lastTextChange = 0;
uint32_t lastColorChange = 0;
char window2Char = '2';
bool window4Blue = false;
bool running = false;

void printError(const char *where) {
  Serial.print(where);
  Serial.print(": ");
  Serial.println(AMT630A_OSD::errorToString(display.getLastError()));
}

bool setupWindows() {
  if (!display.beginBatchUpdate()) return false;
  bool ok = true;

  for (uint8_t i = 0; i < WINDOW_COUNT && ok; ++i) {
    ok = display.configureWindow(i, WINDOW_W, WINDOW_H) &&
         display.setDisplayOrigin(i, windows[i].x, windows[i].y) &&
         display.fillRect(i, 0, 0, WINDOW_COLS, WINDOW_ROWS, baseColor[i]);

    char label[2] = { char('0' + i), '\0' };
    if (ok)
      ok = display.writeString(i, 0, 0, label,
                               AMT630A_OSD::White, baseColor[i]) == 1;
    if (ok) ok = display.setVisible(i, true);
  }

  if (ok) ok = display.forceUpdate();
  bool closed = display.endBatchUpdate();
  return ok && closed;
}

void bounceWindow(uint8_t i) {
  MovingWindow &w = windows[i];
  w.x += w.dx;
  w.y += w.dy;

  int16_t maxX = (int16_t)display.getDisplayWidth() - WINDOW_W;
  int16_t maxY = (int16_t)display.getDisplayHeight() - WINDOW_H;
  if (maxX < 0) maxX = 0;
  if (maxY < 0) maxY = 0;

  if (w.x <= 0) { w.x = 0; w.dx = abs(w.dx); }
  else if (w.x >= maxX) { w.x = maxX; w.dx = -abs(w.dx); }

  if (w.y <= 0) { w.y = 0; w.dy = abs(w.dy); }
  else if (w.y >= maxY) { w.y = maxY; w.dy = -abs(w.dy); }
}

bool moveAllWindows() {
  for (uint8_t i = 0; i < WINDOW_COUNT; ++i) bounceWindow(i);

  if (!display.beginBatchUpdate()) return false;
  bool ok = true;
  for (uint8_t i = 0; i < WINDOW_COUNT && ok; ++i)
    ok = display.moveWindow(i, windows[i].x, windows[i].y);
  bool closed = display.endBatchUpdate();
  return ok && closed;
}

bool updateWindow2Text() {
  if (++window2Char > '9') window2Char = '0';
  char label[2] = { window2Char, '\0' };

  if (!display.beginBatchUpdate()) return false;
  bool ok = display.writeString(2, 0, 0, label,
                                AMT630A_OSD::White, AMT630A_OSD::Blue) == 1;
  if (ok) ok = display.forceUpdate(2);
  bool closed = display.endBatchUpdate();
  return ok && closed;
}

bool updateWindow4Color() {
  window4Blue = !window4Blue;
  AMT630A_OSD::Color bg = window4Blue ? AMT630A_OSD::Blue
                                      : AMT630A_OSD::Yellow;

  if (!display.beginBatchUpdate()) return false;
  bool ok = display.fillRect(4, 0, 0, WINDOW_COLS, WINDOW_ROWS, bg);
  char label[2] = { '4', '\0' };
  if (ok)
    ok = display.writeString(4, 0, 0, label,
                             AMT630A_OSD::White, bg) == 1;
  if (ok) ok = display.forceUpdate(4);
  bool closed = display.endBatchUpdate();
  return ok && closed;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("AMT630A_OSD v0.5.5 - AnimatedWindows");

  if (!display.begin()) { printError("begin"); return; }
  if (!display.setFontMode(AMT630A_OSD::LargeFont)) {
    printError("setFontMode"); return;
  }
  if (!setupWindows()) { printError("setupWindows"); return; }

  uint32_t now = millis();
  lastFrame = lastTextChange = lastColorChange = now;
  running = true;
}

void loop() {
  if (!running) return;
  uint32_t now = millis();

  if ((uint32_t)(now - lastFrame) >= FRAME_MS) {
    lastFrame = now;
    if (!moveAllWindows()) { printError("moveAllWindows"); running = false; return; }
  }

  if ((uint32_t)(now - lastTextChange) >= TEXT_CHANGE_MS) {
    lastTextChange = now;
    if (!updateWindow2Text()) { printError("updateWindow2Text"); running = false; return; }
  }

  if ((uint32_t)(now - lastColorChange) >= COLOR_CHANGE_MS) {
    lastColorChange = now;
    if (!updateWindow4Color()) { printError("updateWindow4Color"); running = false; return; }
  }
}