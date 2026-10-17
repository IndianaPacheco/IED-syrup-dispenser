#include "Display.h"

#include <Wire.h>

// Custom LCD character 1: a small bracket, so a space typed in a
// password is visible. ('_' can't be used: it is a character too.)
static constexpr char SPACE_GLYPH = '\x01';
static uint8_t spaceGlyph[8] = {
  0b00000, 0b00000, 0b00000, 0b00000,
  0b00000, 0b10001, 0b11111, 0b00000
};

Display::Display() : lcd_(LCD_I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS) {}

void Display::begin() {
  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
  lcd_.init();
  lcd_.backlight();
  lcd_.createChar(SPACE_GLYPH, spaceGlyph);
}

void Display::showStartup() {
  printLine(0, "Syrup Dispenser");
  printLine(1, "");
  printLine(2, "Initializing...");
  printLine(3, "");
}

void Display::showWebAddress(const char* address) {
  printLine(2, address[0] ? "Web page at:" : "No WiFi: knob only");
  printLine(3, address);
}

void Display::showMenu(int index, int count, const char* recipeName,
                       int selected) {
  // A name that fits on one row gets "Drink 3/120" under it; a
  // longer one wraps onto that row instead.
  if ((int)strlen(recipeName) > LCD_COLUMNS) {
    printWrapped(0, recipeName);
  } else {
    char counter[LCD_COLUMNS + 1];
    snprintf(counter, sizeof(counter), "Drink %d/%d", index + 1, count);
    printLine(0, recipeName);
    printLine(1, counter);
  }

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
  printLine(3, "");
}

void Display::showDone() {
  printLine(0, "Drink complete");
  printLine(1, "Enjoy!");
  printLine(2, "");
  printLine(3, "");
}

void Display::showCancelled() {
  printLine(0, "DISPENSING STOPPED");
  printLine(1, "All pumps off");
  printLine(2, "");
  printLine(3, "");
}

void Display::showRemoteOrder(const char* recipeName) {
  printLine(0, "Web order:");
  printWrapped(1, recipeName);
  printLine(3, "Cup in, press knob");
}

void Display::showNewDrinkItem() {
  printLine(0, "+ New drink");
  printLine(1, "");
  printLine(2, "");
  printLine(3, "");
}

void Display::showSettingsItem() {
  printLine(0, "Settings");
  printLine(1, "WiFi, web PIN,");
  printLine(2, "web address");
  printLine(3, "");
}

void Display::showList(const char* title, const char* const items[],
                       int count, int selected) {
  printLine(0, title);

  // Show three items at a time, keeping the selected one in view
  // (in the middle, where possible).
  const int rows = LCD_ROWS - 1;
  int first = constrain(selected - 1, 0, max(count - rows, 0));
  for (int row = 0; row < rows; row++) {
    int i = first + row;
    if (i < count) {
      printOption(row + 1, items[i], i == selected);
    } else {
      printLine(row + 1, "");
    }
  }
}

void Display::showMessage(const char* text) {
  printLine(0, text);
  printLine(1, "");
  printLine(2, "");
  printLine(3, "");
}

void Display::showNetworkInfo(const char* network, const char* address,
                              const char* pin) {
  printLine(0, network);
  printLine(1, address);
  printLine(2, pin);
  printLine(3, "");
}

void Display::showTextPicker(const char* label, const char* text,
                             const char* chars, int selected,
                             const char* nextPage) {
  printTextLine(label, text, true);

  // Two columns per character, as in showLetterPicker(). The page
  // switch sits at the right end of the bottom row:
  //   " a b c d e f g h i j"
  //   " k l m n o p q r s t"
  //   " u v w x y z    >ABC"
  const int perRow = LCD_COLUMNS / 2;
  const int count = strlen(chars);

  for (int row = 1; row < LCD_ROWS; row++) {
    char line[LCD_COLUMNS + 1];
    int length = 0;

    for (int col = 0; col < perRow; col++) {
      int i = (row - 1) * perRow + col;
      if (i >= count) {
        break;
      }
      line[length++] = (i == selected) ? '>' : ' ';
      line[length++] = (chars[i] == ' ') ? SPACE_GLYPH : chars[i];
    }
    line[length] = '\0';

    if (row == LCD_ROWS - 1) {
      snprintf(line + length, sizeof(line) - length, "%*c%s",
               LCD_COLUMNS - 3 - length, selected == count ? '>' : ' ',
               nextPage);
    }
    printLine(row, line);
  }
}

void Display::showTextCommands(const char* label, const char* text,
                               int selected, const char* warning) {
  if (warning[0]) {
    printLine(0, warning);
  } else {
    printTextLine(label, text, true);
  }
  printOption(1, "Delete letter", selected == 0);
  printOption(2, "Done",          selected == 1);
  printOption(3, "Cancel",        selected == 2);
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
  printLine(3, "");
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
  printTextLine("Name:", name, false);
}

void Display::printTextLine(const char* label, const char* text,
                            bool markSpaces) {
  // Long text scrolls: show only the end, so the newest letter
  // stays in view. Room is what's left after the label and the '_'.
  const int room = LCD_COLUMNS - strlen(label) - 1;
  int length = strlen(text);
  const char* tail = (length > room) ? text + length - room : text;

  char line[LCD_COLUMNS + 1];
  int at = snprintf(line, sizeof(line), "%s", label);

  for (const char* c = tail; *c && at < LCD_COLUMNS - 1; c++) {
    line[at++] = (*c == ' ' && markSpaces) ? SPACE_GLYPH : *c;
  }
  line[at++] = '_';
  line[at] = '\0';
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
