/*
  Display.h
  ---------
  Everything shown on the 20x4 LCD. The rest of the program says
  WHAT to show (e.g. showDone()); this module decides how it looks.
*/

#pragma once

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "Config.h"

class Display {
public:
  Display();

  // Start the I2C bus and turn on the LCD backlight.
  void begin();

  void showStartup();
  // Where to find the barista web page, under the startup title.
  // An empty address means WiFi didn't connect.
  void showWebAddress(const char* address);
  // A drink with Pour / Edit / Delete / Back under it. selected is
  // the option with the ">" (0..3), or -1 for no marker (browsing).
  void showMenu(int index, int count, const char* recipeName, int selected);
  // No / Yes (selected = 0..1).
  void showDeleteConfirm(const char* recipeName, int selected);
  void showDispensing(int activePumps);
  void showDone();
  void showCancelled();

  // A drink ordered from the web page, waiting for the knob press.
  void showRemoteOrder(const char* recipeName);

  // Menu entry after all the drinks.
  void showNewDrinkItem();

  // Last menu entry.
  void showSettingsItem();

  // Settings screens (see SettingsMenu).

  // A title, then a scrolling list with a ">" on 'selected'.
  void showList(const char* title, const char* const items[], int count,
                int selected);

  // One line of text, e.g. "Scanning WiFi...".
  void showMessage(const char* text);

  // The Web address screen: network, address, PIN.
  void showNetworkInfo(const char* network, const char* address,
                       const char* pin);

  // Typing text (see TextEntry): label and text on top, then the
  // page's characters, then a ">" spot for the page switch showing
  // 'nextPage'. selected == strlen(chars) marks the switch.
  void showTextPicker(const char* label, const char* text, const char* chars,
                      int selected, const char* nextPage);

  // Delete letter / Done / Cancel (selected = 0..2). A non-empty
  // warning replaces the text on the top row.
  void showTextCommands(const char* label, const char* text, int selected,
                        const char* warning);

  // Drink programming screens (see DrinkEditor). A ">" marks the
  // selected letter or option.

  // Name on top, then 'letters' laid out over the other three rows.
  // A space is drawn as '_'.
  void showLetterPicker(const char* name, const char* letters, int selected);

  // Delete letter / Finish name / Exit no save (selected = 0..2).
  void showNameCommands(const char* name, int selected);

  void showAmountEntry(int pump, float amountMl);

  // Save drink / Edit drink / Exit no save (selected = 0..2).
  void showFinishMenu(const char* name, bool canSave, int selected);

  // Top row reads "ERROR", then the message, then an optional hint
  // on the bottom row.
  void showError(const char* message, const char* hint = "");

private:
  // Clear one row and print text, cut off at the screen width.
  void printLine(int row, const char* text);

  // Print text across two rows, wrapping long names onto the second.
  void printWrapped(int row, const char* text);

  // "Name:" plus the end of the name and a '_' cursor, on row 0.
  void printNameLine(const char* name);

  // The same for any label, e.g. "Pass:". markSpaces draws spaces
  // as a visible mark, for passwords.
  void printTextLine(const char* label, const char* text, bool markSpaces);

  // One menu option, with ">" in front if it is selected.
  void printOption(int row, const char* text, bool selected);

  // Two options side by side on one row, e.g. ">Pour     Edit".
  void printOptionPair(int row, const char* left, bool leftSelected,
                       const char* right, bool rightSelected);

  LiquidCrystal_I2C lcd_;
};
