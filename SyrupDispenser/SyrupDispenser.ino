/*
  AUTOMATIC SYRUP DISPENSER
  Controller: ESP32 DevKit V1
  Storage:    SPI microSD card (recipes.jsonl)
  Interface:  20x4 I2C LCD + KY-040 rotary encoder,
              plus a barista web page over WiFi
  Outputs:    8 active-HIGH MOSFET pump drivers

  Files in this sketch:
    Config.h            pins, calibration, limits  <- edit this one
    PumpController.*    switches pumps on/off for timed doses
    RecipeManager.*     reads recipes from the microSD card
    Display.*           everything shown on the LCD
    Input.*             debounced buttons + rotary encoder
    DrinkEditor.*       programming new drinks with the knob
    WebRemote.*         WiFi + the barista web page (WebPage.h)
    Settings.*          WiFi name/password and web PIN, saved on the
                        SD card (never in the code or on GitHub)
    SettingsMenu.*      the Settings menu for those, knob only
    TextEntry.*         typing passwords with the knob
    SyrupDispenser.ino  (this file) ties them together

  How it works (state machine):
    Each drink in MENU is shown with Pour / Edit / Delete / Back under
    it. Turning the knob browses drinks. Pressing enters OPTIONS on
    the same screen: a ">" appears and turning moves it instead.

    MENU --press--> OPTIONS --Pour--> DISPENSING --finishes--> DONE
                       |                    |
                  Back or stop            stop --> CANCELLED
                       v
                     MENU          DONE / CANCELLED / ERROR --press--> MENU

    OPTIONS --Edit--> PROGRAM (drink filled in) --saved--> MENU
    OPTIONS --Delete--> DELETE CONFIRM --Yes--> MENU (drink removed)

    After the drinks comes "+ New drink":
    MENU --press--> PROGRAM (name -> pump amounts -> finish) --saved--> MENU
                       knob only: "Exit no save" returns to MENU

    The last menu entry is "Settings" (WiFi network, web PIN, web
    address), saved to the SD card:
    MENU --press--> SETTINGS --Back--> MENU

    Web orders and web drink changes (Drinks tab: add, edit, delete)
    are only taken while nobody is using the knob (MENU, DONE,
    CANCELLED, ERROR). Nothing pours until someone at the machine
    puts a cup down and presses the knob:
    web order --> REMOTE ORDER --press--> DISPENSING
                       |
                     stop (button or web page) --> CANCELLED

  IMPORTANT:
  1. Test with pump power disconnected first.
  2. Calibrate PUMP_ML_PER_SECOND in Config.h before real drinks.
  3. The stop button is a software stop, NOT a safety-rated E-stop.
     Add a hardware way to cut pump power for a real build.

  Libraries (Arduino IDE Library Manager):
    - LiquidCrystal I2C
    - ArduinoJson by Benoit Blanchon, version 7
    (WiFi, WebServer, and ESPmDNS come with the ESP32 board package.)
*/

#include "Config.h"
#include "Display.h"
#include "DrinkEditor.h"
#include "Input.h"
#include "PumpController.h"
#include "RecipeManager.h"
#include "Settings.h"
#include "SettingsMenu.h"
#include "WebRemote.h"

// ============================================================
// MODULES
// ============================================================

PumpController  pumps;
RecipeManager   recipes;
Display         display;
DrinkEditor     editor;
RotaryEncoder   encoder(ENCODER_CLK_PIN, ENCODER_DT_PIN);
DebouncedButton selectButton(ENCODER_SW_PIN, INPUT);  // KY-040 onboard pull-up
DebouncedButton stopButton(STOP_BUTTON_PIN, INPUT_PULLUP);
NetworkSettings netSettings;  // loaded from the SD card in setup()
WebRemote       remote(recipes, netSettings);
SettingsMenu    settingsMenu(display, remote);

// ============================================================
// STATE
// ============================================================

enum class State {
  Menu,           // scrolling through recipes (no ">" shown)
  Options,        // ">" on Pour / Edit / Delete / Back
  DeleteConfirm,  // "Delete this drink?" No / Yes
  Dispensing,     // pumps running
  Done,           // drink finished
  Cancelled,      // stopped by the stop button
  Program,        // creating or editing a drink in the DrinkEditor
  RemoteOrder,    // web order shown; waiting for the knob press
  Settings,       // in the SettingsMenu
  Error,          // problem with one recipe; press to return to menu
  Fatal           // startup failed (no SD card); needs reset
};

// Options under each drink, in knob order.
enum Option { OPTION_POUR, OPTION_EDIT, OPTION_DELETE, OPTION_BACK, OPTION_COUNT };

State state = State::Menu;
int selectedRecipe = 0;      // index into the menu; past the drinks
                             // come "+ New drink" and "Settings"
int optionChoice = 0;        // where the ">" is in OPTIONS / DELETE CONFIRM
int editingRecipe = -1;      // recipe open in the editor; -1 = new drink
int shownActivePumps = -1;   // last pump count drawn on the LCD

// ============================================================
// STATE TRANSITIONS
// ============================================================

// The menu lists every recipe, "+ New drink" while there is room,
// and "Settings".
int menuLength() {
  return recipes.count() + (recipes.isFull() ? 0 : 1) + 1;
}

bool isNewDrinkSelected() {
  return selectedRecipe == recipes.count() && !recipes.isFull();
}

bool isSettingsSelected() {
  return selectedRecipe == menuLength() - 1;
}

// Draw the selected drink, with the ">" only while in OPTIONS.
void drawMenu() {
  if (isSettingsSelected()) {
    display.showSettingsItem();
  } else if (isNewDrinkSelected()) {
    display.showNewDrinkItem();
  } else {
    int marker = (state == State::Options) ? optionChoice : -1;
    display.showMenu(selectedRecipe, recipes.count(),
                     recipes.name(selectedRecipe), marker);
  }
}

void goToMenu() {
  state = State::Menu;
  drawMenu();
}

void showOptions() {
  state = State::Options;
  optionChoice = OPTION_POUR;
  drawMenu();
}

void showDeleteConfirm() {
  state = State::DeleteConfirm;
  optionChoice = 0;  // "No", so a stray press can't delete
  display.showDeleteConfirm(recipes.name(selectedRecipe), optionChoice);
}

void startProgramming() {
  state = State::Program;
  editingRecipe = -1;
  editor.start();
  editor.draw(display);
}

void openSettings() {
  state = State::Settings;
  settingsMenu.start(netSettings);
  settingsMenu.draw();
}

void handleSettingsResult(SettingsMenu::Result result) {
  switch (result) {
    case SettingsMenu::Result::Open:
      break;
    case SettingsMenu::Result::Save:
      netSettings = settingsMenu.settings();
      if (!saveSettings(netSettings)) {
        showRecipeError("Settings not saved");
        return;
      }
      remote.reconnect();
      break;
    case SettingsMenu::Result::Exit:
      goToMenu();
      return;
  }
  settingsMenu.draw();
}

void startEditing() {
  float amountsMl[PUMP_COUNT];
  if (!recipes.loadAmounts(selectedRecipe, amountsMl)) {
    showRecipeError("Bad recipe data");
    return;
  }

  state = State::Program;
  editingRecipe = selectedRecipe;
  editor.start(recipes.name(selectedRecipe), amountsMl);
  editor.draw(display);
}

void saveProgrammedDrink() {
  if (editingRecipe >= 0) {
    // An edited drink keeps its place in the menu.
    if (!recipes.replaceRecipe(editingRecipe, editor.name(), editor.amounts())) {
      showRecipeError("Save failed");
      return;
    }
    selectedRecipe = editingRecipe;
  } else {
    if (!recipes.addRecipe(editor.name(), editor.amounts())) {
      showRecipeError("Save failed");
      return;
    }
    // Show the new drink, which is now the last recipe.
    selectedRecipe = recipes.count() - 1;
  }
  goToMenu();
}

void deleteSelectedDrink() {
  if (!recipes.deleteRecipe(selectedRecipe)) {
    showRecipeError("Delete failed");
    return;
  }

  // Stay on the drink that moved into this spot (or the one before,
  // if the last drink was deleted).
  if (selectedRecipe >= recipes.count() && recipes.count() > 0) {
    selectedRecipe = recipes.count() - 1;
  }
  goToMenu();
}

void handleEditorResult(DrinkEditor::Result result) {
  switch (result) {
    case DrinkEditor::Result::Editing:
      editor.draw(display);
      break;
    case DrinkEditor::Result::Save:
      saveProgrammedDrink();
      break;
    case DrinkEditor::Result::Cancel:
      goToMenu();
      break;
  }
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

// Knob turned: scroll the menu, move the ">" marker, or change the
// value being edited. Everything wraps around at both ends.
void onEncoderTurn(int step) {
  switch (state) {
    case State::Program:
      editor.turn(step);
      editor.draw(display);
      break;

    case State::Settings:
      settingsMenu.turn(step);
      settingsMenu.draw();
      break;

    case State::Menu: {
      int count = menuLength();
      selectedRecipe = (selectedRecipe + step + count) % count;
      goToMenu();
      break;
    }

    case State::Options:
      optionChoice = (optionChoice + step + OPTION_COUNT) % OPTION_COUNT;
      drawMenu();
      break;

    case State::DeleteConfirm:
      optionChoice = (optionChoice + step + 2) % 2;
      display.showDeleteConfirm(recipes.name(selectedRecipe), optionChoice);
      break;

    default:
      break;
  }
}

// Knob pressed: move forward through the menu flow.
void onSelectPress() {
  switch (state) {
    case State::Menu:
      if (isSettingsSelected()) {
        openSettings();
      } else if (isNewDrinkSelected()) {
        startProgramming();
      } else {
        showOptions();
      }
      break;

    case State::Options:
      switch (optionChoice) {
        case OPTION_POUR:   startDispensing();   break;
        case OPTION_EDIT:   startEditing();      break;
        case OPTION_DELETE: showDeleteConfirm(); break;
        case OPTION_BACK:   goToMenu();          break;
      }
      break;

    case State::DeleteConfirm:
      if (optionChoice == 1) {  // "Yes"
        deleteSelectedDrink();
      } else {
        showOptions();
      }
      break;

    case State::Program:
      handleEditorResult(editor.press());
      break;

    case State::RemoteOrder:
      startDispensing();
      break;

    case State::Settings:
      handleSettingsResult(settingsMenu.press());
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

// Stop pressed: abort dispensing or back out of the options screens.
// The drink editor is knob-only, so stop does nothing there.
void onStopPress() {
  if (state == State::Dispensing || state == State::RemoteOrder) {
    cancelDrink();
  } else if (state == State::Options || state == State::DeleteConfirm) {
    goToMenu();
  }
}

// Stop the drink being poured, or the web order waiting to pour.
void cancelDrink() {
  pumps.stopAll();
  state = State::Cancelled;
  display.showCancelled();
  Serial.println("Dispensing cancelled.");
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
// WEB PAGE HANDLERS (called from remote.poll())
// ============================================================

// True while nobody is using the knob and nothing is pouring, so
// the web page may take over. Rewriting the recipe file can take
// long enough to throw off a pour's timing, so drink changes wait
// for this too.
bool machineIsFree() {
  return state == State::Menu || state == State::Done ||
         state == State::Cancelled || state == State::Error;
}

// The page's list is stale if this drink was edited or deleted on
// the machine (or another browser) after the page loaded.
bool pageMatches(int index, const char* name) {
  return index >= 0 && index < recipes.count() &&
         strcmp(recipes.name(index), name) == 0;
}

// The drink list changed from the web. Keep the menu on a real
// entry and redraw it if it is showing.
void refreshMenu() {
  if (selectedRecipe >= menuLength()) {
    selectedRecipe = menuLength() - 1;
  }
  if (state == State::Menu) {
    drawMenu();
  }
}

// A drink ordered from the web page. It is only shown on the LCD;
// someone at the machine still has to press the knob to pour.
OrderResult onRemoteOrder(int index, const char* name) {
  if (!machineIsFree()) {
    return OrderResult::Busy;
  }
  if (!pageMatches(index, name)) {
    return OrderResult::Changed;
  }

  selectedRecipe = index;
  state = State::RemoteOrder;
  display.showRemoteOrder(recipes.name(index));
  Serial.print("Web order: ");
  Serial.println(recipes.name(index));
  return OrderResult::Accepted;
}

// Stop on the web page. Unlike the stop button, it can't back out
// of the menus, so it can't disturb someone using the knob.
bool onRemoteStop() {
  if (state == State::Dispensing || state == State::RemoteOrder) {
    cancelDrink();
    return true;
  }
  return false;
}

// Add (index -1) or replace a drink from the page's Drinks tab.
EditResult onRemoteSave(int index, const char* oldName, const char* name,
                        const float amountsMl[PUMP_COUNT]) {
  if (!machineIsFree()) {
    return EditResult::Busy;
  }

  if (index < 0) {
    if (recipes.isFull()) {
      return EditResult::Full;
    }
    if (!recipes.addRecipe(name, amountsMl)) {
      return EditResult::Failed;
    }
  } else {
    if (!pageMatches(index, oldName)) {
      return EditResult::Changed;
    }
    if (!recipes.replaceRecipe(index, name, amountsMl)) {
      return EditResult::Failed;
    }
  }

  Serial.print("Drink saved from the web: ");
  Serial.println(name);
  refreshMenu();
  return EditResult::Done;
}

EditResult onRemoteDelete(int index, const char* oldName) {
  if (!machineIsFree()) {
    return EditResult::Busy;
  }
  if (!pageMatches(index, oldName)) {
    return EditResult::Changed;
  }

  Serial.print("Drink deleted from the web: ");
  Serial.println(oldName);
  if (!recipes.deleteRecipe(index)) {
    return EditResult::Failed;
  }

  // Stay on the same drink if the deleted one was before it.
  if (index < selectedRecipe) {
    selectedRecipe--;
  }
  refreshMenu();
  return EditResult::Done;
}

RemoteStatus remoteStatus() {
  const char* drink = recipes.name(selectedRecipe);
  switch (state) {
    case State::Menu:        return {"idle", ""};
    case State::RemoteOrder: return {"waiting", drink};
    case State::Dispensing:  return {"pouring", drink};
    case State::Done:        return {"done", drink};
    case State::Cancelled:   return {"cancelled", ""};
    case State::Error:       return {"error", ""};
    case State::Fatal:       return {"offline", ""};
    default:                 return {"busy", ""};  // knob in use
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

  // Zero recipes is fine: the menu still offers "+ New drink".
  if (!recipes.buildIndex()) {
    fail("Can't read recipes");
    return;
  }

  // WiFi details are on the card. Without them WiFi stays off until
  // set up in the Settings menu.
  loadSettings(netSettings);
  remote.begin({onRemoteOrder, onRemoteStop, remoteStatus,
                onRemoteSave, onRemoteDelete});
  if (netSettings.mode != WifiMode::Off) {
    display.showWebAddress(remote.address().c_str());
    delay(WEB_ADDRESS_SHOW_MS);  // long enough to read it
  }

  selectedRecipe = 0;
  goToMenu();
  Serial.println("Syrup dispenser ready.");
}

void loop() {
  // Answer the web page first. Its stop goes through the same
  // cancelDrink() as the stop button.
  remote.poll();

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

  // The Web address screen updates itself once WiFi connects.
  if (state == State::Settings && settingsMenu.needsRedraw()) {
    settingsMenu.draw();
  }
}
