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
  void showMenu(int index, int count, const char* recipeName);
  void showConfirm(const char* recipeName);
  void showDispensing(int activePumps);
  void showDone();
  void showCancelled();

  // Last menu entry, after all the drinks.
  void showNewDrinkItem();

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
  // on the bottom row (e.g. "Press to return").
  void showError(const char* message, const char* hint = "");

private:
  // Clear one row and print text, cut off at the screen width.
  void printLine(int row, const char* text);

  // Print text across two rows, wrapping long names onto the second.
  void printWrapped(int row, const char* text);

  // "Name:" plus the end of the name and a '_' cursor, on row 0.
  void printNameLine(const char* name);

  // One menu option, with ">" in front if it is selected.
  void printOption(int row, const char* text, bool selected);

  LiquidCrystal_I2C lcd_;
};
