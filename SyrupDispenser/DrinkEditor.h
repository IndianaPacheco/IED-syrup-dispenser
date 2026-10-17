/*
  DrinkEditor.h
  -------------
  Lets the user program a new drink using only the encoder knob
  (turn to move the ">" marker, press to pick):

    1. NAME     the alphabet a-z and space fill the screen. Press to
                add the marked letter. Turn past the letters for
                Delete letter / Finish name / Exit no save.
    2. AMOUNTS  for each pump 1..8, turn to set the mL, press for next.
    3. FINISH   Save drink / Edit drink (back to the name) / Exit no save.

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
    Cancel    // user chose to exit without saving
  };

  // Clear everything and begin at the name step.
  void start();

  // Begin at the name step with an existing drink filled in, and the
  // marker on "Finish name" so pressing goes straight to the amounts.
  void start(const char* name, const float amountsMl[PUMP_COUNT]);

  // Knob turned (+1 / -1).
  void turn(int step);

  // Knob pressed: accept the current choice.
  Result press();

  // Redraw the current step on the LCD.
  void draw(Display& display) const;

  const char*  name() const    { return name_; }
  const float* amounts() const { return amountsMl_; }

private:
  enum class Step { Name, Amounts, Finish };

  // True if at least one pump has an amount above 0 mL.
  bool hasAmounts() const;

  // Drop trailing spaces from the name.
  void trimName();

  // As long as RecipeManager can store.
  static constexpr int NAME_LIMIT = MAX_NAME_LENGTH - 1;

  Step     step_ = Step::Name;
  char     name_[NAME_LIMIT + 1] = "";
  int      nameLength_ = 0;
  int      choice_ = 0;     // where the ">" marker is on this step
  int      pump_ = 0;       // pump being set (0-based)
  float    amountsMl_[PUMP_COUNT] = {};
  uint32_t lastTurnMs_ = 0;
};
