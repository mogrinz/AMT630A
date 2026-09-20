#include <AMT630A_OSD.h>

AMT630A_OSD display;

void printError(const char *where) {
  Serial.print(where);
  Serial.print(": ");
  Serial.println(AMT630A_OSD::errorToString(display.getLastError()));
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("AMT630A_OSD v0.4.1 - Multiple Windows Example");
  Serial.println("------------------------------------------------");

  if (!display.begin()) {
    printError("begin");
    return;
  }

  // Global text geometry for all text windows.
  if (!display.setFontMode(AMT630A_OSD::LargeFont)) {
    printError("setFontMode");
    return;
  }

  // ----------------------------------------------------------
  // Each window is 12 columns x 4 rows.
  //
  // LargeFont = 16 x 22 pixels per character
  //
  // 20 * 16 = 320 pixels wide
  //  4 * 22 =  88 pixels high
  //
  // 48 Index RAM cells per window.
  // ----------------------------------------------------------

  if (!display.configureWindow(0, 320, 88)) {
    printError("configureWindow 0");
    return;
  }

  if (!display.configureWindow(1, 320, 88)) {
    printError("configureWindow 1");
    return;
  }

  // Safe positions with comfortable margins.
  display.setDisplayOrigin(0, 30, 30);
  display.setDisplayOrigin(1, 30, 150);

  // ----------------------------------------------------------
  // WINDOW 0
  // ----------------------------------------------------------

  display.clearWindow(0);

  display.writeString(
      0, 0, 0,
      "TOP WINDOW",
      AMT630A_OSD::White);

  display.writeString(
      0, 0, 1,
      "WINDOW 0",
      AMT630A_OSD::Cyan);

  display.writeString(
      0, 0, 2,
      "INDEX RAM 0",
      AMT630A_OSD::Green);

  display.writeString(
      0, 0, 3,
      "SEPARATE BUFFER",
      AMT630A_OSD::Yellow);


  // ----------------------------------------------------------
  // WINDOW 1
  // ----------------------------------------------------------

  display.clearWindow(1);

  display.writeString(
      1, 0, 0,
      "BOTTOM WINDOW",
      AMT630A_OSD::White);

  display.writeString(
      1, 0, 1,
      "WINDOW 1",
      AMT630A_OSD::Yellow);

  display.writeString(
      1, 0, 2,
      "INDEX RAM 1",
      AMT630A_OSD::Cyan);

  display.writeString(
      1, 0, 3,
      "SEPARATE BUFFER",
      AMT630A_OSD::Green);


  // ----------------------------------------------------------
  // Make both windows visible.
  // ----------------------------------------------------------

  display.setVisible(0, true);
  display.setVisible(1, true);

  if (!display.forceUpdate()) {
    printError("forceUpdate");
    return;
  }

  Serial.println();
  Serial.println("Both windows configured.");

  Serial.print("Window 0 Index start: 0x");
  Serial.println(display.getWindowIndexStart(0), HEX);

  Serial.print("Window 1 Index start: 0x");
  Serial.println(display.getWindowIndexStart(1), HEX);

  Serial.print("Index RAM used: ");
  Serial.println(display.getIndexRAMUsed());

  Serial.print("Index RAM free: ");
  Serial.println(display.getIndexRAMFree());
}

void loop() {
  display.service();
}