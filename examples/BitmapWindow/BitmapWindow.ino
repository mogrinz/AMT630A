#include <AMT630A_OSD.h>
#include "SampleBitmap.h"

AMT630A_OSD display;
AMT630A_OSD::BitmapHandle bitmapHandle;

void printError(const char *where) {
  Serial.print(where);
  Serial.print(": ");
  Serial.println(AMT630A_OSD::errorToString(display.getLastError()));
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("AMT630A_OSD v0.5.1 - Bitmap Window Example");
  Serial.println("-------------------------------------------");

  if (!display.begin()) {
    printError("begin");
    return;
  }

  // v0.5.1 bitmap support uses the LargeFont 16x22 global geometry.
  if (!display.setFontMode(AMT630A_OSD::LargeFont)) {
    printError("setFontMode");
    return;
  }

  // Reserve zero custom glyphs: make the full 8 KB Font RAM available
  // to bitmap resources.
  if (!display.configureFontRAM(0)) {
    printError("configureFontRAM");
    return;
  }

  Serial.print("Bitmap memory available: ");
  Serial.print(display.getBitmapMemoryFreeBytes());
  Serial.println(" bytes");

  // A normal ROM-font text window can coexist with the bitmap window.
  if (!display.configureWindow(0, 320, 44)) {
    printError("configureWindow 0");
    return;
  }
  display.setDisplayOrigin(0, 30, 30);
  display.clearWindow(0);
  display.writeString(0, 0, 0, "BITMAP TEST", AMT630A_OSD::White);
  display.writeString(0, 0, 1, "16 COLOR", AMT630A_OSD::Cyan);

  // Loading writes the generated 4bpp data into Font RAM and, by default,
  // installs the bitmap's generated 16-entry palette.
  if (!display.loadBitmap(sampleBitmap, bitmapHandle)) {
    printError("loadBitmap");
    return;
  }

  if (!display.configureBitmapWindow(1, bitmapHandle)) {
    printError("configureBitmapWindow");
    return;
  }
  display.setDisplayOrigin(1, 200, 100);

  if (!display.forceUpdate()) {
    printError("forceUpdate");
    return;
  }

  Serial.print("Bitmap storage used: ");
  Serial.print(display.getBitmapStorageBytes(bitmapHandle));
  Serial.println(" bytes");

  Serial.print("Bitmap memory remaining: ");
  Serial.print(display.getBitmapMemoryFreeBytes());
  Serial.println(" bytes");
}

void loop() {
  display.service();
}