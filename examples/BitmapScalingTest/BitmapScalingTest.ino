#include <AMT630A_OSD.h>
#include "robot.h"

AMT630A_OSD display;
AMT630A_OSD::BitmapHandle robotHandle;

uint8_t selectedWindow = 0;
bool visible[2] = { true, true };

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
  Serial.println("  0 / 1 = select W0 or W1");
  Serial.println("  q     = 1x horizontal + vertical");
  Serial.println("  w     = 2x horizontal + vertical");
  Serial.println("  e     = 3x horizontal + vertical");
  Serial.println("  r     = 4x horizontal + vertical");
  Serial.println("  h     = cycle horizontal scale");
  Serial.println("  v     = cycle vertical scale");
  Serial.println("  t     = toggle other robot visible/hidden");
  Serial.println("  ?     = help");
  Serial.println();
}

bool createBitmapWindows()
{
  if (!display.setFontMode(AMT630A_OSD::LargeFont)) {
    printError("setFontMode");
    return false;
  }

  if (!display.configureFontRAM(0)) {
    printError("configureFontRAM");
    return false;
  }

  if (!display.loadBitmap(robot, robotHandle)) {
    printError("loadBitmap");
    return false;
  }

  if (!display.beginBatchUpdate()) {
    printError("beginBatchUpdate");
    return false;
  }

  bool ok = true;

  if (ok) ok = display.configureBitmapWindow(0, robotHandle);
  if (ok) ok = display.setDisplayOrigin(0, 30, 30);
  if (ok) ok = display.setVisible(0, true);

  if (ok) ok = display.configureBitmapWindow(1, robotHandle);
  if (ok) ok = display.setDisplayOrigin(1, 210, 30);
  if (ok) ok = display.setVisible(1, true);

  if (ok) ok = display.forceUpdate();

  const bool closed = display.endBatchUpdate();

  if (!ok) {
    printError("createBitmapWindows");
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
  Serial.println("AMT630A BITMAP SCALING TEST");
  Serial.println("===========================");

  if (!display.begin()) {
    printError("begin");
    return;
  }

  if (!createBitmapWindows())
    return;

  // W0 exercises the AMT630A's special mask-based scaler.
  // W1 exercises the normal per-window coefficient scaler.
  if (!display.setWindowScale(0, 1) ||
      !display.setWindowScale(1, 2)) {
    printError("setWindowScale");
    return;
  }

  Serial.println("Robot bitmap loaded once and shared by W0/W1.");
  Serial.println("Initial scales: W0 = 1x, W1 = 2x");

  printWindowState();
  printHelp();
}

void loop()
{
  display.service();

  if (!Serial.available())
    return;

  const char c = Serial.read();

  if (c == '\r' || c == '\n')
    return;

  if (c == '0' || c == '1') {
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

  if (c == 't' || c == 'T') {
    const uint8_t other = (selectedWindow == 0) ? 1 : 0;
    visible[other] = !visible[other];

    if (!display.setVisible(other, visible[other]))
      printError("setVisible");

    Serial.print("W");
    Serial.print(other);
    Serial.print(" = ");
    Serial.println(visible[other] ? "VISIBLE" : "HIDDEN");
    return;
  }

  printHelp();
}