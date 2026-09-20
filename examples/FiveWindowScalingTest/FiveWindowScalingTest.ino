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

static const uint8_t WINDOW_COUNT = 5;
static const uint8_t COLS = 2;
static const uint8_t ROWS = 1;
static const uint16_t CELL_W = 16;
static const uint16_t CELL_H = 22;
static const uint16_t WINDOW_W = COLS * CELL_W;
static const uint16_t WINDOW_H = ROWS * CELL_H;

struct WindowPos {
  uint16_t x;
  uint16_t y;
};

WindowPos positions[WINDOW_COUNT] = {
  { 20, 20 },
  { 180, 20 },
  { 20, 150 },
  { 180, 150 },
  { 360, 150 }
};

uint8_t selectedWindow = 0;

void printError(const char *where)
{
  Serial.print(where);
  Serial.print(": ");
  Serial.println(AMT630A_OSD::errorToString(display.getLastError()));
}

void printWindowState()
{
  Serial.print("Selected W");
  Serial.print(selectedWindow);
  Serial.print("  X=");
  Serial.print(display.getWindowScaleX(selectedWindow));
  Serial.print("x  Y=");
  Serial.print(display.getWindowScaleY(selectedWindow));
  Serial.println("x");
}

void printHelp()
{
  Serial.println();
  Serial.println("Controls:");
  Serial.println("  0..4 = select window");
  Serial.println("  q    = 1x horizontal + vertical");
  Serial.println("  w    = 2x horizontal + vertical");
  Serial.println("  e    = 3x horizontal + vertical");
  Serial.println("  r    = 4x horizontal + vertical");
  Serial.println("  h    = cycle horizontal scale");
  Serial.println("  v    = cycle vertical scale");
  Serial.println("  ?    = help");
  Serial.println();
}

bool createWindows()
{
  if (!display.setFontMode(AMT630A_OSD::LargeFont)) {
    printError("setFontMode");
    return false;
  }

  const AMT630A_OSD::Color backgrounds[WINDOW_COUNT] = {
    AMT630A_OSD::Red,
    AMT630A_OSD::Green,
    AMT630A_OSD::Blue,
    AMT630A_OSD::Yellow,
    AMT630A_OSD::White
  };

  const AMT630A_OSD::Color foregrounds[WINDOW_COUNT] = {
    AMT630A_OSD::White,
    AMT630A_OSD::White,
    AMT630A_OSD::White,
    AMT630A_OSD::Blue,
    AMT630A_OSD::Blue
  };

  if (!display.beginBatchUpdate()) {
    printError("beginBatchUpdate");
    return false;
  }

  bool ok = true;

  for (uint8_t i = 0; i < WINDOW_COUNT && ok; ++i) {
    ok = display.configureWindow(i, WINDOW_W, WINDOW_H);

    if (ok)
      ok = display.setDisplayOrigin(i, positions[i].x, positions[i].y);

    if (ok)
      ok = display.fillRect(i, 0, 0, COLS, ROWS, backgrounds[i]);

    if (ok)
      ok = display.writeString(
        i, 0, 0, "AB", foregrounds[i], backgrounds[i]) == 2;

    if (ok)
      ok = display.setVisible(i, true);
  }

  if (ok) ok = display.forceUpdate();

  const bool closed = display.endBatchUpdate();

  if (!ok) {
    printError("createWindows");
    return false;
  }

  if (!closed) {
    printError("endBatchUpdate");
    return false;
  }

  return true;
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("AMT630A FIVE-WINDOW SCALING TEST");
  Serial.println("================================");

  if (!display.begin()) {
    printError("begin");
    return;
  }

  if (!createWindows())
    return;

  // Same unified API for every window, even though W0 uses different
  // scaler hardware internally.
  if (!display.setWindowScale(0, 1) ||
      !display.setWindowScale(1, 2) ||
      !display.setWindowScale(2, 3) ||
      !display.setWindowScale(3, 4) ||
      !display.setWindowScale(4, 1)) {
    printError("setWindowScale");
    return;
  }

  Serial.println("Initial scales:");
  Serial.println("  W0 = 1x");
  Serial.println("  W1 = 2x");
  Serial.println("  W2 = 3x");
  Serial.println("  W3 = 4x");
  Serial.println("  W4 = 1x");

  printWindowState();
  printHelp();
}

void loop()
{
  if (!Serial.available())
    return;

  const char c = Serial.read();

  if (c == '\r' || c == '\n')
    return;

  if (c >= '0' && c <= '4') {
    selectedWindow = c - '0';
    printWindowState();
    return;
  }

  uint8_t scale = 0;
  if (c == 'q' || c == 'Q') scale = 1;
  else if (c == 'w' || c == 'W') scale = 2;
  else if (c == 'e' || c == 'E') scale = 3;
  else if (c == 'r' || c == 'R') scale = 4;

  if (scale) {
    if (!display.setWindowScale(selectedWindow, scale))
      printError("setWindowScale");
    printWindowState();
    return;
  }

  if (c == 'h' || c == 'H') {
    uint8_t sx = display.getWindowScaleX(selectedWindow) + 1;
    if (sx > 4) sx = 1;

    if (!display.setWindowScale(
          selectedWindow,
          sx,
          display.getWindowScaleY(selectedWindow)))
      printError("setWindowScale");

    printWindowState();
    return;
  }

  if (c == 'v' || c == 'V') {
    uint8_t sy = display.getWindowScaleY(selectedWindow) + 1;
    if (sy > 4) sy = 1;

    if (!display.setWindowScale(
          selectedWindow,
          display.getWindowScaleX(selectedWindow),
          sy))
      printError("setWindowScale");

    printWindowState();
    return;
  }

  printHelp();
}