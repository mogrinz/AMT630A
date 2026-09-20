#include <AMT630A_OSD.h>

AMT630A_OSD display;

// -----------------------------------------------------------------------------
// MultipleWindowBatchTest
//
// Diagnostic for AMT630A_OSD v0.5.3 batch updates.
//
// Five overlapping 5x5 LargeFont windows move horizontally at different
// speeds. All five position changes for a frame occur inside one
// beginBatchUpdate()/endBatchUpdate() transaction so the AMT630A sees only
// one C6 unlock/relock cycle per frame.
//
// Hardware observation from earlier testing:
//   z-order is 0 (top), then 1, 2, 3, 4.
// -----------------------------------------------------------------------------

static const uint8_t WINDOW_COUNT = 5;
static const uint8_t WINDOW_COLS = 5;
static const uint8_t WINDOW_ROWS = 5;
static const uint16_t WINDOW_W = WINDOW_COLS * 16;   // LargeFont: 16 px
static const uint16_t WINDOW_H = WINDOW_ROWS * 22;   // LargeFont: 22 px

static const int16_t LEFT_EDGE = 20;
static const int16_t RIGHT_EDGE_EXTRA = 20;
static const int16_t FIRST_Y = 20;
static const int16_t Y_STEP = 20;
static const uint32_t FRAME_MS = 60;

struct WindowMotion {
  int16_t x;
  int16_t y;
  int8_t direction;
  uint8_t step;
  AMT630A_OSD::Color background;
};

WindowMotion windows[WINDOW_COUNT] = {
  {  20,  20,  1, 1, AMT630A_OSD::Red    },
  {  85,  40,  1, 2, AMT630A_OSD::Green  },
  { 150,  60, -1, 3, AMT630A_OSD::Blue   },
  { 215,  80, -1, 4, AMT630A_OSD::Cyan   },
  { 280, 100, -1, 5, AMT630A_OSD::Yellow }
};

uint32_t lastFrame = 0;

void printError(const char *where)
{
  Serial.print(where);
  Serial.print(": ");
  Serial.println(AMT630A_OSD::errorToString(display.getLastError()));
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("AMT630A_OSD v0.5.3 - MultipleWindowBatchTest");
  Serial.println("------------------------------------------------");
  Serial.println("Five overlapping windows; one C6 transaction per frame.");

  if (!display.begin()) {
    printError("begin");
    return;
  }

  if (!display.setFontMode(AMT630A_OSD::LargeFont)) {
    printError("setFontMode");
    return;
  }

  // Configure all windows first.
  for (uint8_t i = 0; i < WINDOW_COUNT; ++i) {
    if (!display.configureWindow(i, WINDOW_W, WINDOW_H)) {
      printError("configureWindow");
      return;
    }
  }

  // Then position and draw them after Index RAM allocation has settled.
  for (uint8_t i = 0; i < WINDOW_COUNT; ++i) {
    windows[i].y = FIRST_Y + (i * Y_STEP);

    if (!display.setDisplayOrigin(i, windows[i].x, windows[i].y)) {
      printError("setDisplayOrigin");
      return;
    }

    if (!display.fillRect(i, 0, 0, WINDOW_COLS, WINDOW_ROWS,
                          windows[i].background)) {
      printError("fillRect");
      return;
    }

    char label[2] = { char('0' + i), '\0' };
    if (display.writeString(i, 0, 0, label,
                            AMT630A_OSD::White,
                            windows[i].background) != 1) {
      printError("writeString");
      return;
    }

    if (!display.setVisible(i, true)) {
      printError("setVisible");
      return;
    }
  }

  if (!display.forceUpdate()) {
    printError("forceUpdate");
    return;
  }

  Serial.print("Index RAM used: ");
  Serial.println(display.getIndexRAMUsed());
  Serial.println("Starting batched movement...");
  lastFrame = millis();
}

void loop()
{
  display.service();

  const uint32_t now = millis();
  if ((uint32_t)(now - lastFrame) < FRAME_MS)
    return;
  lastFrame = now;

  const int16_t rightEdge =
      (int16_t)display.getDisplayWidth() + RIGHT_EDGE_EXTRA - WINDOW_W;

  // First calculate the whole next frame in RAM.
  for (uint8_t i = 0; i < WINDOW_COUNT; ++i) {
    WindowMotion &w = windows[i];
    w.x += w.direction * w.step;

    if (w.x >= rightEdge) {
      w.x = rightEdge;
      w.direction = -1;
    }
    else if (w.x <= LEFT_EDGE) {
      w.x = LEFT_EDGE;
      w.direction = 1;
    }
  }

  // Then send all five position changes under ONE C6 unlock/relock.
  if (!display.beginBatchUpdate()) {
    printError("beginBatchUpdate");
    return;
  }

  bool ok = true;
  for (uint8_t i = 0; i < WINDOW_COUNT; ++i) {
    if (!display.moveWindow(i, windows[i].x, windows[i].y)) {
      Serial.print("moveWindow ");
      Serial.print(i);
      Serial.print(": ");
      Serial.println(AMT630A_OSD::errorToString(display.getLastError()));
      ok = false;
      break;
    }
  }

  // Always close a batch that successfully began, even after a move error.
  if (!display.endBatchUpdate()) {
    printError("endBatchUpdate");
    return;
  }

  if (!ok)
    return;
}