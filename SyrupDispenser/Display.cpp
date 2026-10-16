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
  printLine(1, "");
  printLine(2, "Initializing...");
  printLine(3, "");
}

void Display::showMenu(int index, int count, const char* recipeName) {
  // e.g. "Drink 3/120". Short enough to fit even at 1000/1000.
  char header[LCD_COLUMNS + 1];
  int length = snprintf(header, sizeof(header), "Drink %d/%d", index + 1, count);
  if (length >= (int)sizeof(header)) {
    Serial.println("Menu header truncated.");
  }

  printLine(0, header);
  printWrapped(1, recipeName);
  printLine(3, "Turn=browse Press=ok");
}

void Display::showConfirm(const char* recipeName) {
  printLine(0, "Ready to pour:");
  printWrapped(1, recipeName);
  printLine(3, "Press=pour Stop=back");
}

void Display::showDispensing(int activePumps) {
  char status[LCD_COLUMNS + 1];
  snprintf(status, sizeof(status), "%d pumps active", activePumps);

  printLine(0, "Dispensing...");
  printLine(1, status);
  printLine(2, "");
  printLine(3, "Stop to cancel");
}

void Display::showDone() {
  printLine(0, "Drink complete");
  printLine(1, "Enjoy!");
  printLine(2, "");
  printLine(3, "Press to return");
}

void Display::showCancelled() {
  printLine(0, "DISPENSING STOPPED");
  printLine(1, "All pumps off");
  printLine(2, "");
  printLine(3, "Press to return");
}

void Display::showNewDrinkItem() {
  printLine(0, "+ New drink");
  printLine(1, "Program a recipe");
  printLine(2, "");
  printLine(3, "Press to program");
}

void Display::showNameEntry(const char* name, const char* choice) {
  // Name so far with an underscore as the cursor, e.g. "Mocha_".
  char nameLine[LCD_COLUMNS + 1];
  snprintf(nameLine, sizeof(nameLine), "%s_", name);

  char pickLine[LCD_COLUMNS + 1];
  snprintf(pickLine, sizeof(pickLine), "Pick: %s", choice);

  printLine(0, "Name your drink:");
  printLine(1, nameLine);
  printLine(2, pickLine);
  printLine(3, "Press=add Stop=exit");
}

void Display::showAmountEntry(int pump, float amountMl) {
  char top[24];  // printLine() cuts it to the screen width
  snprintf(top, sizeof(top), "Pump %d of %d", pump + 1, PUMP_COUNT);

  char amount[LCD_COLUMNS + 1];
  snprintf(amount, sizeof(amount), "%.1f mL", amountMl);

  printLine(0, top);
  printLine(1, amount);
  printLine(2, "");
  printLine(3, "Press=next Stop=back");
}

void Display::showSaveConfirm(const char* name) {
  printLine(0, "Save this drink?");
  printWrapped(1, name);
  printLine(3, "Press=save Stop=back");
}

void Display::showNothingToSave() {
  printLine(0, "All pumps are 0 mL");
  printLine(1, "Nothing to save");
  printLine(2, "");
  printLine(3, "Stop to go back");
}

void Display::showError(const char* message, const char* hint) {
  printLine(0, "ERROR");
  printWrapped(1, message);
  printLine(3, hint);
}

void Display::printLine(int row, const char* text) {
  // Pad the line with spaces instead of calling lcd.clear(),
  // which is slow and makes the screen flicker.
  char padded[LCD_COLUMNS + 1];
  snprintf(padded, sizeof(padded), "%-*s", LCD_COLUMNS, text);

  lcd_.setCursor(0, row);
  lcd_.print(padded);
}

void Display::printWrapped(int row, const char* text) {
  printLine(row, text);

  // Whatever didn't fit on the first row goes on the next one.
  const char* rest = "";
  if ((int)strlen(text) > LCD_COLUMNS) {
    rest = text + LCD_COLUMNS;
  }
  printLine(row + 1, rest);
}
