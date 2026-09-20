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
  if (!display.setFontMode(AMT630A_OSD::SmallFont)) { printError("setFontMode"); return; }

  // Window 0: READY, upper-left.
  if (!display.configureWindow(0, 100, 16, AMT630A_OSD::TextMode)) { printError("configureWindow W0"); return; }
  if (!display.setDisplayOrigin(0, 26, 20)) { printError("setDisplayOrigin W0"); return; }
  display.writeString(0, 0, 0, "READY", AMT630A_OSD::Green, AMT630A_OSD::Transparent);

  // Window 1: POWER and a five-cell power bar, upper-right.
  if (!display.configureWindow(1, 170, 16, AMT630A_OSD::TextMode)) { printError("configureWindow W1"); return; }
  if (!display.setDisplayOrigin(1, 342, 20)) { printError("setDisplayOrigin W1"); return; }
  display.writeString(1, 0, 0, "POWER", AMT630A_OSD::Green, AMT630A_OSD::Transparent);
  display.fillRect(1, 6, 0, 5, 1, AMT630A_OSD::Green);

  // Three equal 12x3-cell status windows with one transparent row between
  // the two displayed status rows.
  for (uint8_t w = 2; w <= 4; ++w) {
    if (!display.configureWindow(w, 120, 48, AMT630A_OSD::TextMode)) { printError("configureWindow status"); return; }
  }

  if (!display.setDisplayOrigin(2, 38, 226)) { printError("setDisplayOrigin W2"); return; }
  if (!display.setDisplayOrigin(3, 198, 226)) { printError("setDisplayOrigin W3"); return; }
  if (!display.setDisplayOrigin(4, 358, 226)) { printError("setDisplayOrigin W4"); return; }

  display.writeString(2, 0, 0, "   GYROS    ", AMT630A_OSD::White, AMT630A_OSD::Green);
  display.writeString(2, 0, 2, "   RADAR    ", AMT630A_OSD::White, AMT630A_OSD::Green);

  display.writeString(3, 0, 0, "  SAX KEYS  ", AMT630A_OSD::White, AMT630A_OSD::Red);
  display.writeString(3, 0, 2, "  HEARTBOX  ", AMT630A_OSD::White, AMT630A_OSD::Green);

  display.writeString(4, 0, 0, " PIANO KEYS ", AMT630A_OSD::White, AMT630A_OSD::Red);
  display.writeString(4, 0, 2, "   LIGHTS   ", AMT630A_OSD::White, AMT630A_OSD::Green);

  for (uint8_t i = 0; i < AMT630A_OSD::WINDOW_COUNT; ++i) {
    if (!display.setVisible(i, true)) { printError("setVisible"); return; }
  }

  if (!display.forceUpdate()) printError("forceUpdate");
}

void loop() {
  display.service();
}