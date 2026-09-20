#include <AMT630A_OSD.h>
#include "robot.h"

AMT630A_OSD display;
AMT630A_OSD::BitmapHandle robotHandle;

// Animation tuning.
static const uint32_t MOVE_INTERVAL_MS = 30;
static const uint16_t MOVE_STEP_PIXELS = 1;

uint16_t robotX = 0;
uint16_t robotY = 0;
int8_t direction = 1;
uint32_t lastMoveMs = 0;

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
  Serial.println("AMT630A MOVING ROBOT BITMAP TEST");
  Serial.println("--------------------------------");

  if (!display.begin()) {
    printError("begin");
    return;
  }

  // Bitmap mode currently uses the verified 16x22 LargeFont geometry.
  if (!display.setFontMode(AMT630A_OSD::LargeFont)) {
    printError("setFontMode");
    return;
  }

  // Give the entire Font RAM bitmap area to bitmap resources.
  if (!display.configureFontRAM(0)) {
    printError("configureFontRAM");
    return;
  }

  // robot.h is intentionally not supplied with this library example.
  // Generate/copy your own robot.h into this example directory.
  if (!display.loadBitmap(robot, robotHandle)) {
    printError("loadBitmap");
    return;
  }

  if (!display.configureBitmapWindow(0, robotHandle)) {
    printError("configureBitmapWindow");
    return;
  }

  // Position the logical bitmap against the bottom edge of the configured
  // display. Protect the subtraction in case a larger image is substituted.
  const uint16_t displayHeight = display.getDisplayHeight();
  const uint16_t robotHeight = display.getBitmapHeight(robotHandle);
  robotY = (robotHeight < displayHeight) ? (displayHeight - robotHeight) : 0;

  robotX = 0;

  // setDisplayOrigin() is appropriate here because this is one-time setup.
  if (!display.setDisplayOrigin(0, robotX, robotY)) {
    printError("setDisplayOrigin");
    return;
  }

  if (!display.setVisible(0, true)) {
    printError("setVisible");
    return;
  }

  if (!display.forceUpdate()) {
    printError("forceUpdate");
    return;
  }

  Serial.println("Bitmap loaded successfully.");
  Serial.print("Display: ");
  Serial.print(display.getDisplayWidth());
  Serial.print(" x ");
  Serial.println(display.getDisplayHeight());
  Serial.print("Robot: ");
  Serial.print(display.getBitmapWidth(robotHandle));
  Serial.print(" x ");
  Serial.println(display.getBitmapHeight(robotHandle));
  Serial.print("Bottom Y: ");
  Serial.println(robotY);
}

void loop()
{
  // Keep the library's normal visibility/coexistence maintenance running.
  display.service();

  const uint32_t now = millis();
  if ((uint32_t)(now - lastMoveMs) < MOVE_INTERVAL_MS)
    return;

  lastMoveMs = now;

  const uint16_t displayWidth = display.getDisplayWidth();
  const uint16_t robotWidth = display.getBitmapWidth(robotHandle);
  const uint16_t rightEdge =
      (robotWidth < displayWidth) ? (displayWidth - robotWidth) : 0;

  if (direction > 0) {
    if (robotX + MOVE_STEP_PIXELS >= rightEdge) {
      robotX = rightEdge;
      direction = -1;
    } else {
      robotX += MOVE_STEP_PIXELS;
    }
  } else {
    if (robotX <= MOVE_STEP_PIXELS) {
      robotX = 0;
      direction = 1;
    } else {
      robotX -= MOVE_STEP_PIXELS;
    }
  }

  // IMPORTANT: use moveWindow() for animation. It updates only the position
  // bytes that changed and does not reconfigure the whole OSD window.
  if (!display.moveWindow(0, robotX, robotY)) {
    printError("moveWindow");
  }
}