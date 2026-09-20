#include <AMT630A_OSD.h>

AMT630A_OSD display;

const uint16_t happyFace[AMT630A_OSD::CUSTOM_GLYPH_ROWS] = {
  0b000111111000,
  0b001000000100,
  0b010000000010,
  0b100000000001,
  0b100110011001,
  0b100110011001,
  0b100000000001,
  0b100000000001,
  0b100000000001,
  0b100100001001,
  0b100010010001,
  0b100001100001,
  0b100000000001,
  0b010000000010,
  0b001000000100,
  0b000111111000
};

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

  // Reserve 16 custom-glyph slots. The rest of Font RAM is reserved for
  // future bitmap allocation. Capacity is fixed even if only slot 0 is loaded.
  if (!display.configureFontRAM(16)) { printError("configureFontRAM"); return; }

  if (!display.configureWindow(0, 480, 264)) { printError("configureWindow"); return; }
  display.setDisplayOrigin(0, 0, 4);

  if (!display.loadCustomGlyph(0, happyFace)) { printError("loadCustomGlyph"); return; }

  display.clearWindow(0);
  display.writeStringCentered(0, 2, "CUSTOM GLYPH", AMT630A_OSD::White);
  display.writeCustomGlyph(0, 10, 5, 0, AMT630A_OSD::Cyan);
  display.writeCustomGlyph(0, 14, 5, 0, AMT630A_OSD::White);
  display.writeCustomGlyph(0, 18, 5, 0, AMT630A_OSD::Yellow);
  display.writeStringCentered(0, 8, "12 X 16 FONT RAM", AMT630A_OSD::Green);

  if (!display.updateDisplay()) printError("updateDisplay");
}

void loop() {
  display.service();
}