/*
  AUTOMATIC SYRUP DISPENSER
  Controller: ESP32 DevKit V1
  Storage:    SPI microSD card (recipes.jsonl)
  Interface:  16x2 I2C LCD + KY-040 rotary encoder
  Outputs:    8 active-HIGH MOSFET pump drivers

  Files in this sketch:
    Config.h            pins, calibration, limits  <- edit this one
    PumpController.*    switches pumps on/off for timed doses
    RecipeManager.*     reads recipes from the microSD card
    Display.*           everything shown on the LCD
    Input.*             debounced buttons + rotary encoder
    SyrupDispenser.ino  (this file) ties them together

  How it works (state machine):
    MENU --press--> CONFIRM --press--> DISPENSING --finishes--> DONE
                       |                    |
                     stop                 stop --> CANCELLED
                       v
                     MENU          DONE / CANCELLED / ERROR --press--> MENU

  IMPORTANT:
  1. Test with pump power disconnected first.
  2. Calibrate PUMP_ML_PER_SECOND in Config.h before real drinks.
  3. The stop button is a software stop, NOT a safety-rated E-stop.
     Add a hardware way to cut pump power for a real build.

  Libraries (Arduino IDE Library Manager):
    - LiquidCrystal I2C
    - ArduinoJson by Benoit Blanchon, version 7
*/

#include "Config.h"
#include "Display.h"
#include "Input.h"
#include "PumpController.h"
#include "RecipeManager.h"

// ============================================================
// MODULES
// ============================================================

PumpController  pumps;
RecipeManager   recipes;
Display         display;
RotaryEncoder   encoder(ENCODER_CLK_PIN, ENCODER_DT_PIN);
DebouncedButton selectButton(ENCODER_SW_PIN, INPUT);  // external pull-up
DebouncedButton stopButton(STOP_BUTTON_PIN, INPUT_PULLUP);

// ============================================================
// STATE
// ============================================================

enum class State {
  Menu,        // scrolling through recipes
  Confirm,     // recipe chosen, waiting for a second press
  Dispensing,  // pumps running
  Done,        // drink finished
  Cancelled,   // stopped by the stop button
  Error,       // problem with one recipe; press to return to menu
  Fatal        // startup failed (no SD card / recipes); needs reset
};

State state = State::Menu;
int selectedRecipe = 0;      // index into the recipe list
int shownActivePumps = -1;   // last pump count drawn on the LCD

// ============================================================
// STATE TRANSITIONS
// ============================================================

void goToMenu() {
  state = State::Menu;
  display.showMenu(selectedRecipe, recipes.count(),
                   recipes.name(selectedRecipe));
}

// A problem with the chosen recipe. The user can pick another one.
void showRecipeError(const char* message) {
  pumps.stopAll();
  state = State::Error;
  display.showError(message);
  Serial.print("Recipe error: ");
  Serial.println(message);
}

// Unrecoverable startup problem. Pumps stay off until reset.
void fail(const char* message) {
  pumps.stopAll();
  state = State::Fatal;
  display.showError(message);
  Serial.print("Fatal: ");
  Serial.println(message);
}

void startDispensing() {
  float amountsMl[PUMP_COUNT];

  if (!recipes.loadAmounts(selectedRecipe, amountsMl)) {
    showRecipeError("Bad recipe data");
    return;
  }

  switch (pumps.start(amountsMl)) {
    case DispenseResult::Started:
      state = State::Dispensing;
      shownActivePumps = -1;  // force the first screen update
      Serial.print("Dispensing: ");
      Serial.println(recipes.name(selectedRecipe));
      break;
    case DispenseResult::EmptyRecipe:
      showRecipeError("Recipe is empty");
      break;
    case DispenseResult::BadCalibration:
      showRecipeError("Bad calibration");
      break;
    case DispenseResult::DoseTooLong:
      showRecipeError("Dose too long");
      break;
  }
}

// ============================================================
// INPUT HANDLERS
// ============================================================

// Knob turned: scroll the menu (wraps around at both ends).
void onEncoderTurn(int step) {
  if (state != State::Menu) {
    return;
  }

  int count = recipes.count();
  selectedRecipe = (selectedRecipe + step + count) % count;
  goToMenu();
}

// Knob pressed: move forward through the menu flow.
void onSelectPress() {
  switch (state) {
    case State::Menu:
      state = State::Confirm;
      display.showConfirm(recipes.name(selectedRecipe));
      break;

    case State::Confirm:
      startDispensing();
      break;

    case State::Done:
    case State::Cancelled:
    case State::Error:
      goToMenu();
      break;

    case State::Dispensing:  // only the stop button acts here
    case State::Fatal:       // needs a reset
      break;
  }
}

// Stop pressed: abort dispensing, or back out of the confirm screen.
void onStopPress() {
  if (state == State::Dispensing) {
    pumps.stopAll();
    state = State::Cancelled;
    display.showCancelled();
    Serial.println("Dispensing cancelled.");
  } else if (state == State::Confirm) {
    goToMenu();
  }
}

// Called every loop while pumps are running.
void updateDispensing() {
  if (!pumps.update()) {
    state = State::Done;
    display.showDone();
    return;
  }

  // Only redraw when the number changes, to avoid LCD flicker.
  int active = pumps.activeCount();
  if (active != shownActivePumps) {
    shownActivePumps = active;
    display.showDispensing(active);
  }
}

// ============================================================
// SETUP & LOOP
// ============================================================

void setup() {
  // Pumps first, so their outputs are forced OFF as early as possible.
  pumps.begin();

  Serial.begin(115200);

  encoder.begin();
  selectButton.begin();
  stopButton.begin();

  display.begin();
  display.showStartup();

  if (!recipes.beginCard()) {
    fail("SD init failed");
    return;
  }

  if (!recipes.buildIndex()) {
    fail("No valid recipes");
    return;
  }

  selectedRecipe = 0;
  goToMenu();
  Serial.println("Syrup dispenser ready.");
}

void loop() {
  // After a startup failure, do nothing but keep the pumps off.
  if (state == State::Fatal) {
    pumps.stopAll();
    return;
  }

  // Check the stop button first so it always takes priority.
  if (stopButton.wasPressed()) {
    onStopPress();
  }

  int step = encoder.readStep();
  if (step != 0) {
    onEncoderTurn(step);
  }

  if (selectButton.wasPressed()) {
    onSelectPress();
  }

  if (state == State::Dispensing) {
    updateDispensing();
  }
}
