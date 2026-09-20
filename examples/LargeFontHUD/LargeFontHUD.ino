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

  if (!display.begin()) { printError("begin"); return; }
  if (!display.setFontMode(AMT630A_OSD::LargeFont)) { printError("setFontMode"); return; }

  // Window 0: READY, upper-left.
  if (!display.configureWindow(0, 128, 22, AMT630A_OSD::TextMode)) { printError("configureWindow W0"); return; }
  if (!display.setDisplayOrigin(0, 26, 20)) { printError("setDisplayOrigin W0"); return; }
  display.writeString(0, 0, 0, "READY", AMT630A_OSD::Green, AMT630A_OSD::Transparent);

  // Window 1: POWER and a five-cell power bar, upper-right.
  if (!display.configureWindow(1, 192, 22, AMT630A_OSD::TextMode)) { printError("configureWindow W1"); return; }
  if (!display.setDisplayOrigin(1, 282, 20)) { printError("setDisplayOrigin W1"); return; }
  display.writeString(1, 0, 0, "POWER", AMT630A_OSD::Green, AMT630A_OSD::Transparent);
  display.fillRect(1, 6, 0, 5, 1, AMT630A_OSD::Green);

  // Windows 2-4: three status columns. Each is three rows high so row 1 can
  // remain transparent and create a clean gap between the two status rows.
  if (!display.configureWindow(2, 128, 66, AMT630A_OSD::TextMode)) { printError("configureWindow W2"); return; }
  if (!display.configureWindow(3, 160, 66, AMT630A_OSD::TextMode)) { printError("configureWindow W3"); return; }
  if (!display.configureWindow(4, 128, 66, AMT630A_OSD::TextMode)) { printError("configureWindow W4"); return; }

  if (!display.setDisplayOrigin(2, 38, 206)) { printError("setDisplayOrigin W2"); return; }
  if (!display.setDisplayOrigin(3, 182, 206)) { printError("setDisplayOrigin W3"); return; }
  if (!display.setDisplayOrigin(4, 358, 206)) { printError("setDisplayOrigin W4"); return; }

  display.fillRect(2, 0, 0, 8, 1, AMT630A_OSD::Green);
  display.writeString(2, 1, 0, "GYROS", AMT630A_OSD::White, AMT630A_OSD::Green);
  display.fillRect(2, 0, 2, 8, 1, AMT630A_OSD::Green);
  display.writeString(2, 1, 2, "RADAR", AMT630A_OSD::White, AMT630A_OSD::Green);

  display.fillRect(3, 0, 0, 10, 1, AMT630A_OSD::Red);
  display.writeString(3, 1, 0, "SAX KEYS", AMT630A_OSD::White, AMT630A_OSD::Red);
  display.fillRect(3, 0, 2, 10, 1, AMT630A_OSD::Green);
  display.writeString(3, 1, 2, "HEARTBOX", AMT630A_OSD::White, AMT630A_OSD::Green);

  display.fillRect(4, 0, 0, 8, 1, AMT630A_OSD::Red);
  display.writeString(4, 1, 0, "PIANO", AMT630A_OSD::White, AMT630A_OSD::Red);
  display.fillRect(4, 0, 2, 8, 1, AMT630A_OSD::Green);
  display.writeString(4, 1, 2, "LIGHTS", AMT630A_OSD::White, AMT630A_OSD::Green);

  for (uint8_t i = 0; i < AMT630A_OSD::WINDOW_COUNT; ++i) {
    if (!display.setVisible(i, true)) { printError("setVisible"); return; }
  }

  if (!display.forceUpdate()) printError("forceUpdate");
}

void loop() {
  display.service();
}