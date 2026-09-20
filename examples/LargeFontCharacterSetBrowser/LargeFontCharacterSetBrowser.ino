#include <Wire.h>
#include <AMT630A_OSD.h>

AMT630A_OSD display;

static const uint16_t SCAN_MIN = 0x000;
static const uint16_t SCAN_MAX = 0x3FF;
static const uint8_t PAGE_COLS = 12;
static const uint8_t PAGE_ROWS = 5;
static const uint16_t PAGE_COUNT = PAGE_COLS * PAGE_ROWS;
static const uint16_t CELL_W = 16;
static const uint16_t CELL_H = 22;
static const uint8_t WINDOW_COLS = 23;
static const uint8_t WINDOW_ROWS = 9;
static const uint16_t WINDOW_W = WINDOW_COLS * CELL_W;
static const uint16_t WINDOW_H = WINDOW_ROWS * CELL_H;
uint16_t pageBase = SCAN_MIN;

void printError(const char *where) {
  Serial.print(where); Serial.print(": ");
  Serial.println(AMT630A_OSD::errorToString(display.getLastError()));
}

void clampPageBase() {
  const uint16_t maxBase = SCAN_MAX - PAGE_COUNT + 1;
  if (pageBase < SCAN_MIN) pageBase = SCAN_MIN;
  if (pageBase > maxBase) pageBase = maxBase;
}

bool drawPage() {
  if (!display.clearWindow(0)) { printError("clearWindow"); return false; }
  for (uint16_t i = 0; i < PAGE_COUNT; ++i) {
    const uint16_t id = pageBase + i;
    if (id > SCAN_MAX) break;
    const uint8_t x = (i % PAGE_COLS) * 2;
    const uint8_t y = (i / PAGE_COLS) * 2;
    if (!display.writeGlyph(0, x, y, id, AMT630A_OSD::White, AMT630A_OSD::Transparent)) {
      printError("writeGlyph"); return false;
    }
  }
  if (!display.forceUpdate(0)) { printError("forceUpdate"); return false; }

  Serial.println();
  Serial.print("Showing 0x"); Serial.print(pageBase, HEX); Serial.print(" .. 0x");
  uint16_t end = pageBase + PAGE_COUNT - 1; if (end > SCAN_MAX) end = SCAN_MAX;
  Serial.println(end, HEX); Serial.println();
  for (uint8_t row = 0; row < PAGE_ROWS; ++row) {
    Serial.print("  ");
    for (uint8_t col = 0; col < PAGE_COLS; ++col) {
      const uint16_t id = pageBase + (uint16_t)row * PAGE_COLS + col;
      if (id > SCAN_MAX) break;
      Serial.print("0x"); if (id < 0x100) Serial.print('0'); if (id < 0x010) Serial.print('0');
      Serial.print(id, HEX); Serial.print(" ");
    }
    Serial.println();
  }
  Serial.println();
  return true;
}

void printHelp() {
  Serial.println();
  Serial.println("AMT630A LargeFont Character Set Browser");
  Serial.println("] next page   [ previous page   + next ID   - previous ID   ? help");
  Serial.println();
}

void setup() {
  Serial.begin(115200); delay(1000);
  if (!display.begin()) { printError("begin"); return; }
  if (!display.setFontMode(AMT630A_OSD::LargeFont)) { printError("setFontMode"); return; }
  if (!display.configureWindow(0, WINDOW_W, WINDOW_H, AMT630A_OSD::TextMode)) { printError("configureWindow"); return; }
  if (!display.setDisplayOrigin(0, 41, 25)) { printError("setDisplayOrigin"); return; }
  if (!display.setVisible(0, true)) { printError("setVisible"); return; }
  if (!display.forceUpdate()) { printError("forceUpdate"); return; }
  printHelp(); drawPage();
}

void loop() {
  display.service();
  if (!Serial.available()) return;
  const char c = Serial.read();
  if (c == '\r' || c == '\n') return;
  switch (c) {
    case ']': pageBase += PAGE_COUNT; clampPageBase(); drawPage(); break;
    case '[': pageBase = (pageBase >= SCAN_MIN + PAGE_COUNT) ? pageBase - PAGE_COUNT : SCAN_MIN; drawPage(); break;
    case '+': if (pageBase < SCAN_MAX - PAGE_COUNT + 1) ++pageBase; drawPage(); break;
    case '-': if (pageBase > SCAN_MIN) --pageBase; drawPage(); break;
    case '?': printHelp(); break;
    default: printHelp(); break;
  }
}