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

/*
  BlinkDemo
  =========

  Demonstrates the AMT630A's hardware-verified primary blink engine.

  Important: the chip has ONE verified blink engine, so only one OSD window
  can own the blink region at a time. This example keeps three windows visible
  and switches the blink engine between them. Each window has its own demo
  blink region; selecting a window applies that window's stored profile.

  Windows are 7 characters x 3 lines and filled with alphabet characters.

  Serial controls (115200 baud):
    0 = blink Window 0 region
    1 = blink Window 1 region
    2 = blink Window 2 region
    b = toggle blink on/off

  Hardware-verified behavior:
    - multiple windows may remain visible while blinking
    - FB35 selects which OSD window owns the blink region
    - smaller raw blink-rate values blink faster
*/

static const uint8_t COLS = 7;
static const uint8_t ROWS = 3;
static const uint16_t CELL_W = 16;
static const uint16_t CELL_H = 22;
static const uint16_t WINDOW_W = COLS * CELL_W;
static const uint16_t WINDOW_H = ROWS * CELL_H;

struct BlinkProfile {
  uint8_t x;
  uint8_t y;
  uint8_t width;
  uint8_t height;
};

// Hardware-verified blink coordinates are 1-based: first cell is (1,1).
// These three profiles make the active blink region easy to distinguish.
static const BlinkProfile profiles[3] = {
  {1, 1, 3, 3},  // W0: first 3 columns of all 3 rows
  {3, 1, 3, 3},  // W1: middle 3 columns of all 3 rows
  {5, 1, 3, 3}   // W2: last 3 columns of all 3 rows
};

static const uint8_t BLINK_RATE = 0x10;

uint8_t activeBlinkWindow = 0;
bool blinkEnabled = true;

void printError(const char *where) {
  Serial.print("ERROR at ");
  Serial.print(where);
  Serial.print(": ");
  Serial.println(AMT630A_OSD::errorToString(display.getLastError()));
}

bool createWindow(uint8_t n,
                  uint16_t x,
                  uint16_t y,
                  AMT630A_OSD::Color background,
                  const char *row0,
                  const char *row1,
                  const char *row2) {
  if (!display.configureWindow(n, WINDOW_W, WINDOW_H)) {
    printError("configureWindow");
    return false;
  }

  if (!display.setDisplayOrigin(n, x, y)) {
    printError("setDisplayOrigin");
    return false;
  }

  if (!display.fillRect(n, 0, 0, COLS, ROWS, background)) {
    printError("fillRect");
    return false;
  }

  if (display.writeString(n, 0, 0, row0, AMT630A_OSD::White, background) != COLS ||
      display.writeString(n, 0, 1, row1, AMT630A_OSD::White, background) != COLS ||
      display.writeString(n, 0, 2, row2, AMT630A_OSD::White, background) != COLS) {
    printError("writeString");
    return false;
  }

  if (!display.setVisible(n, true)) {
    printError("setVisible");
    return false;
  }

  return true;
}

bool selectBlinkProfile(uint8_t windowNumber) {
  const BlinkProfile &p = profiles[windowNumber];

  if (!display.configureBlink(windowNumber,
                              p.x, p.y,
                              p.width, p.height,
                              BLINK_RATE)) {
    printError("configureBlink");
    return false;
  }

  activeBlinkWindow = windowNumber;

  Serial.print("Blink window: ");
  Serial.print(windowNumber);
  Serial.print("  region x=");
  Serial.print(p.x);
  Serial.print(" y=");
  Serial.print(p.y);
  Serial.print(" w=");
  Serial.print(p.width);
  Serial.print(" h=");
  Serial.println(p.height);

  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("AMT630A OSD BlinkDemo");
  Serial.println("====================");

  if (!display.begin()) {
    printError("begin");
    return;
  }

  if (!display.setFontMode(AMT630A_OSD::LargeFont)) {
    printError("setFontMode");
    return;
  }

  if (!display.beginBatchUpdate()) {
    printError("beginBatchUpdate");
    return;
  }

  bool ok = true;

  if (ok) ok = createWindow(0, 35, 35, AMT630A_OSD::Red,
                            "ABCDEFG", "HIJKLMN", "OPQRSTU");

  if (ok) ok = createWindow(1, 205, 105, AMT630A_OSD::Green,
                            "VWXYZAB", "CDEFGHI", "JKLMNOP");

  if (ok) ok = createWindow(2, 375, 175, AMT630A_OSD::Blue,
                            "QRSTUVW", "XYZABCD", "EFGHIJK");

  if (ok && !display.forceUpdate()) {
    printError("forceUpdate");
    ok = false;
  }

  if (!display.endBatchUpdate()) {
    printError("endBatchUpdate");
    return;
  }

  if (!ok) return;

  // Start immediately with blinking active on Window 0.
  if (!selectBlinkProfile(0)) return;

  if (!display.setBlinkEnabled(true)) {
    printError("setBlinkEnabled");
    return;
  }

  Serial.println();
  Serial.println("Controls:");
  Serial.println("  0 = blink Window 0's region");
  Serial.println("  1 = blink Window 1's region");
  Serial.println("  2 = blink Window 2's region");
  Serial.println("  b = toggle blink on/off");
}

void loop() {
  if (!Serial.available()) return;

  char c = Serial.read();

  if (c >= '0' && c <= '2') {
    selectBlinkProfile((uint8_t)(c - '0'));
  }
  else if (c == 'b' || c == 'B') {
    blinkEnabled = !blinkEnabled;
    if (!display.setBlinkEnabled(blinkEnabled)) {
      printError("setBlinkEnabled");
      return;
    }

    Serial.print("Blink: ");
    Serial.println(blinkEnabled ? "ON" : "OFF");
  }
}