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

void Display::showDrinkOptions(const char* recipeName, int selected) {
  printWrapped(0, recipeName);
  printOptionPair(2, "Pour",   selected == 0, "Edit", selected == 1);
  printOptionPair(3, "Delete", selected == 2, "Back", selected == 3);
}

void Display::showDeleteConfirm(const char* recipeName, int selected) {
  printLine(0, "Delete this drink?");
  printWrapped(1, recipeName);
  printOptionPair(3, "No", selected == 0, "Yes", selected == 1);
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

void Display::showLetterPicker(const char* name, const char* letters,
                               int selected) {
  printNameLine(name);

  // Each letter takes two columns: the ">" marker spot, then the
  // letter. That fits 10 per row:
  //   " a b c d e f g h i j"
  //   " k l m n o p q r s t"
  //   " u v w x y z _ more"
  const int perRow = LCD_COLUMNS / 2;
  const int count = strlen(letters);

  for (int row = 1; row < LCD_ROWS; row++) {
    char line[LCD_COLUMNS + 1];
    int length = 0;

    for (int col = 0; col < perRow; col++) {
      int i = (row - 1) * perRow + col;
      if (i >= count) {
        break;
      }
      line[length++] = (i == selected) ? '>' : ' ';
      line[length++] = (letters[i] == ' ') ? '_' : letters[i];
    }
    line[length] = '\0';

    // Hint that turning further leads to Delete / Finish / Exit.
    if (row == LCD_ROWS - 1) {
      snprintf(line + length, sizeof(line) - length, " more");
    }
    printLine(row, line);
  }
}

void Display::showNameCommands(const char* name, int selected) {
  printNameLine(name);
  printOption(1, "Delete letter", selected == 0);
  printOption(2, "Finish name",   selected == 1);
  printOption(3, "Exit no save",  selected == 2);
}

void Display::showAmountEntry(int pump, float amountMl) {
  char top[24];  // printLine() cuts it to the screen width
  snprintf(top, sizeof(top), "Pump %d of %d", pump + 1, PUMP_COUNT);

  char amount[LCD_COLUMNS + 1];
  snprintf(amount, sizeof(amount), "%.1f mL", amountMl);

  printLine(0, top);
  printLine(1, amount);
  printLine(2, "");
  printLine(3, "Turn=mL Press=next");
}

void Display::showFinishMenu(const char* name, bool canSave, int selected) {
  // An all-zero drink can't be saved, so say why on top.
  printLine(0, canSave ? name : "All pumps are 0 mL");
  printOption(1, "Save drink",   selected == 0);
  printOption(2, "Edit drink",   selected == 1);
  printOption(3, "Exit no save", selected == 2);
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

void Display::printNameLine(const char* name) {
  // Long names scroll: show only the end, so the newest letter
  // stays in view. Room is what's left after "Name:" and the '_'.
  const int room = LCD_COLUMNS - 6;
  int length = strlen(name);
  const char* tail = (length > room) ? name + length - room : name;

  char line[LCD_COLUMNS + 1];
  snprintf(line, sizeof(line), "Name:%s_", tail);
  printLine(0, line);
}

void Display::printOption(int row, const char* text, bool selected) {
  char line[LCD_COLUMNS + 1];
  snprintf(line, sizeof(line), "%c%s", selected ? '>' : ' ', text);
  printLine(row, line);
}

void Display::printOptionPair(int row, const char* left, bool leftSelected,
                              const char* right, bool rightSelected) {
  // The right option starts halfway across the screen.
  char line[LCD_COLUMNS + 1];
  snprintf(line, sizeof(line), "%c%-*s%c%s",
           leftSelected ? '>' : ' ', LCD_COLUMNS / 2 - 1, left,
           rightSelected ? '>' : ' ', right);
  printLine(row, line);
}
