#include "Display.h"

#include <Wire.h>

Display::Display() : lcd_(LCD_I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS) {}

void Display::begin() {
  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
  lcd_.init();
  lcd_.backlight();
}

void Display::showStartup() {
  printLine(0, "Syrup Dispenser");
  printLine(1, "Initializing...");
}

void Display::showMenu(int index, int count, const char* recipeName) {
  // e.g. "Drink 3/120". Short enough to fit even at 1000/1000.
  char header[LCD_COLUMNS + 1];
  int length = snprintf(header, sizeof(header), "Drink %d/%d", index + 1, count);
  if (length >= (int)sizeof(header)) {
    Serial.println("Menu header truncated.");
  }

  printLine(0, header);
  printLine(1, recipeName);
}

void Display::showConfirm(const char* recipeName) {
  printLine(0, "Press to start");
  printLine(1, recipeName);
}

void Display::showDispensing(int activePumps) {
  char status[LCD_COLUMNS + 1];
  snprintf(status, sizeof(status), "%d pumps active", activePumps);

  printLine(0, "Dispensing...");
  printLine(1, status);
}

void Display::showDone() {
  printLine(0, "Drink complete");
  printLine(1, "Press to return");
}

void Display::showCancelled() {
  printLine(0, "DISPENSING STOP");
  printLine(1, "Press to return");
}

void Display::showError(const char* message) {
  printLine(0, "ERROR");
  printLine(1, message);
}

void Display::printLine(int row, const char* text) {
  // Pad the line with spaces instead of calling lcd.clear(),
  // which is slow and makes the screen flicker.
  char padded[LCD_COLUMNS + 1];
  snprintf(padded, sizeof(padded), "%-*s", LCD_COLUMNS, text);

  lcd_.setCursor(0, row);
  lcd_.print(padded);
}
