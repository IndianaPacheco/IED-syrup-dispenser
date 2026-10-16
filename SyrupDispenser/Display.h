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

  // Drink programming screens (see DrinkEditor).
  void showNameEntry(const char* name, const char* choice);
  void showAmountEntry(int pump, float amountMl);
  void showSaveConfirm(const char* name);
  void showNothingToSave();

  // Top row reads "ERROR", then the message, then an optional hint
  // on the bottom row (e.g. "Press to return").
  void showError(const char* message, const char* hint = "");

private:
  // Clear one row and print text, cut off at the screen width.
  void printLine(int row, const char* text);

  // Print text across two rows, wrapping long names onto the second.
  void printWrapped(int row, const char* text);

  LiquidCrystal_I2C lcd_;
};
