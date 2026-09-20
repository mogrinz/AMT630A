/*
Library Name: AMT630A On-Screen-Display (OSD)
Created by: Michael Ogrinz, September 2026
Contact: mike@ogrinz.com, https://youtube.com/@ogrinz_labs
Description: A library for creating on-screen text and bitmap graphics over a live composite video signal for devices using the AMT630A video decoder and digital TFT-LCD panel-control SoC.
Repository: https://github.com/mogrinz/AMT630A
License: MIT License
Changelog:
v0.7.0 - 2026-09-20: Initial release.
*/

#include <AMT630A_OSD.h>

AMT630A_OSD display;

// ESP32 GPIOs connected to the 74HC4066 control inputs.
static const uint8_t PLUS_PIN  = 5;   // 4066 pin 13; switch pins 1 & 2
static const uint8_t MENU_PIN  = 18;  // 4066 pin 5;  switch pins 3 & 4
static const uint8_t MINUS_PIN = 19;  // 4066 pin 6;  switch pins 8 & 9

void printError(const char *where) {
  Serial.print(where);
  Serial.print(": ");
  Serial.println(AMT630A_OSD::errorToString(display.getLastError()));
}

void setup() {
  Serial.begin(115200);

  // Give the stock AMT630A firmware time to finish starting up.
  delay(5000);

  // This keypad interface and factory-menu sequence are hardware/firmware
  // specific. They are NOT required for normal AMT630A_OSD library use.
  if (!display.configureFactoryMenuControl(MENU_PIN, PLUS_PIN, MINUS_PIN)) {
    printError("configureFactoryMenuControl");
    return;
  }

  // Hardware testing found 100 ms press / 150 ms gap to be the shortest
  // repeatable timing on this display. Use some margin here.
  if (!display.setFactoryMenuTiming(150, 200)) {
    printError("setFactoryMenuTiming");
    return;
  }

  // Uses the library's default sequence:
  // MENU, MENU, MENU, -, MENU, +, -, -, +, +, MENU
  //
  // To support a different firmware, call setFactoryMenuSequence() first.
  if (!display.enterFactoryMode()) {
    printError("enterFactoryMode");
    return;
  }

  // Allow the Factory screen to settle before beginning I2C OSD access.
  delay(2000);

  if (!display.begin()) {
    printError("begin");
    return;
  }

  if (!display.setFontMode(AMT630A_OSD::SmallFont)) {
    printError("setFontMode");
    return;
  }

  if (!display.configureWindow(0, 480, 80)) {
    printError("configureWindow");
    return;
  }

  if (!display.setDisplayOrigin(0, 32, 80)) {
    printError("setDisplayOrigin");
    return;
  }

    
  // Fill the entire character window with blue
  display.fillRect(0, 0, 0, 34, 4, AMT630A_OSD::Blue);

  display.writeString(0, 1, 0, "IT IS NOW SAFE TO TALK TO", AMT630A_OSD::White, AMT630A_OSD::Blue);
  display.writeString(0, 1, 1, "THE AMT630A WITHOUT INTERFERENCE", AMT630A_OSD::White, AMT630A_OSD::Blue);
  display.writeString(0, 1, 2, "FROM THE DEFAULT FIRMWARE", AMT630A_OSD::White, AMT630A_OSD::Blue);

  if (!display.updateDisplay()) {
    printError("updateDisplay");
    return;
  }
}

void loop() {
  display.service();
}