#include <AMT630A_OSD.h>

AMT630A_OSD display;

static const uint8_t WINDOW_COUNT = 5;
static const uint8_t COLS = 8;
static const uint8_t ROWS = 3;
static const uint16_t CELL_W = 16;
static const uint16_t CELL_H = 22;
static const uint16_t WINDOW_W = COLS * CELL_W;
static const uint16_t WINDOW_H = ROWS * CELL_H;

struct WindowDef {
  uint16_t x;
  uint16_t y;
  AMT630A_OSD::Color background;
  AMT630A_OSD::Color foreground;
  const char *label;
};

WindowDef windows[WINDOW_COUNT] = {
  {  10,  25, AMT630A_OSD::Red,    AMT630A_OSD::White, "WINDOW 0" },
  { 176,  25, AMT630A_OSD::Green,  AMT630A_OSD::White, "WINDOW 1" },
  { 342,  25, AMT630A_OSD::Blue,   AMT630A_OSD::White, "WINDOW 2" },
  {  90, 145, AMT630A_OSD::Yellow, AMT630A_OSD::Blue,  "WINDOW 3" },
  { 250, 145, AMT630A_OSD::White,  AMT630A_OSD::Blue,  "WINDOW 4" }
};

void printError(const char *where)
{
  Serial.print(where);
  Serial.print(": ");
  Serial.println(AMT630A_OSD::errorToString(display.getLastError()));
}

void printHelp()
{
  Serial.println();
  Serial.println("Controls:");
  Serial.println("  0..7 = set global opacity");
  Serial.println("  +    = brightness +1");
  Serial.println("  -    = brightness -1");
  Serial.println("  x    = toggle blending");
  Serial.println();
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("AMT630A FIVE-WINDOW ALPHA TEST");
  Serial.println("------------------------------");

  if (!display.begin()) {
    printError("begin");
    return;
  }

  if (!display.setFontMode(AMT630A_OSD::LargeFont)) {
    printError("setFontMode");
    return;
  }

  // Configure/draw all five windows in one short C6 batch.
  if (!display.beginBatchUpdate()) {
    printError("beginBatchUpdate");
    return;
  }

  bool ok = true;

  for (uint8_t i = 0; i < WINDOW_COUNT && ok; ++i) {
    ok = display.configureWindow(i, WINDOW_W, WINDOW_H);
    if (ok) ok = display.setDisplayOrigin(i, windows[i].x, windows[i].y);
    if (ok) ok = display.fillRect(i, 0, 0, COLS, ROWS, windows[i].background);
    if (ok) {
      ok = display.writeString(
             i, 0, 1, windows[i].label,
             windows[i].foreground,
             windows[i].background) == 8;
    }
    if (ok) ok = display.setVisible(i, true);
  }

  if (ok) ok = display.forceUpdate();

  if (!display.endBatchUpdate()) {
    printError("endBatchUpdate");
    return;
  }

  if (!ok) {
    printError("window setup");
    return;
  }

  // Hardware-verified global OSD/video blend control.
  // Opacity 0 = transparent; 7 = maximum OSD opacity.
  if (!display.setOpacity(4)) {
    printError("setOpacity");
    return;
  }

  if (!display.setBlendingEnabled(true)) {
    printError("setBlendingEnabled");
    return;
  }

  Serial.println("Five windows active at global opacity 4.");
  Serial.println("All windows should change opacity together.");
  Serial.print("Brightness = ");
  Serial.println(display.getBrightness());

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

  if (c >= '0' && c <= '7') {
    const uint8_t opacity = c - '0';
    if (!display.setOpacity(opacity)) {
      printError("setOpacity");
      return;
    }
    Serial.print("Opacity = ");
    Serial.println(display.getOpacity());
  }
  else if (c == '+') {
    uint8_t brightness = display.getBrightness();
    if (brightness < 31) ++brightness;
    if (!display.setBrightness(brightness)) {
      printError("setBrightness");
      return;
    }
    Serial.print("Brightness = ");
    Serial.println(display.getBrightness());
  }
  else if (c == '-') {
    uint8_t brightness = display.getBrightness();
    if (brightness > 0) --brightness;
    if (!display.setBrightness(brightness)) {
      printError("setBrightness");
      return;
    }
    Serial.print("Brightness = ");
    Serial.println(display.getBrightness());
  }
  else if (c == 'x' || c == 'X') {
    const bool enabled = !display.isBlendingEnabled();
    if (!display.setBlendingEnabled(enabled)) {
      printError("setBlendingEnabled");
      return;
    }
    Serial.print("Blending = ");
    Serial.println(display.isBlendingEnabled() ? "ENABLED" : "DISABLED");
  }
  else {
    printHelp();
  }
}