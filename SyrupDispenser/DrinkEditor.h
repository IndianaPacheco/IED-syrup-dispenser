/*
  DrinkEditor.h
  -------------
  Lets the user program a new drink with the knob and buttons:

    1. NAME     turn to pick a letter, press to add it.
                Pick [Done] to finish or [Delete] to erase a letter.
    2. AMOUNTS  for each pump 1..8, turn to set the mL, press for next.
    3. SAVE     press to save the drink to the SD card.

  The stop button goes back one step (and cancels from the name step).

  This module only collects the drink; RecipeManager saves it.
*/

#pragma once

#include <Arduino.h>
#include "Config.h"
#include "Display.h"

class DrinkEditor {
public:
  // What the main program should do after an input.
  enum class Result {
    Editing,  // still editing; nothing to do
    Save,     // user confirmed; save name() and amounts()
    Cancel    // user backed out of the editor
  };

  // Clear everything and begin at the name step.
  void start();

  // Knob turned (+1 / -1).
  void turn(int step);

  // Knob pressed: accept the current choice.
  Result press();

  // Stop pressed: go back one step.
  Result back();

  // Redraw the current step on the LCD.
  void draw(Display& display) const;

  const char*  name() const    { return name_; }
  const float* amounts() const { return amountsMl_; }

private:
  enum class Step { Name, Amounts, Confirm };

  // True if at least one pump has an amount above 0 mL.
  bool hasAmounts() const;

  // Names are kept to one LCD row, leaving room for the cursor.
  static constexpr int NAME_LIMIT = LCD_COLUMNS - 1;

  Step     step_ = Step::Name;
  char     name_[NAME_LIMIT + 1] = "";
  int      nameLength_ = 0;
  int      choice_ = 0;     // selected entry in the character list
  int      pump_ = 0;       // pump being set (0-based)
  float    amountsMl_[PUMP_COUNT] = {};
  uint32_t lastTurnMs_ = 0;
};
