#include "DrinkEditor.h"

// Choices on the name screen. Index 0 and 1 are commands; the rest
// are the characters that can be added to the name.
static constexpr int CHOICE_DONE   = 0;
static constexpr int CHOICE_DELETE = 1;
static const char NAME_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789 -&'.";
static constexpr int NAME_CHAR_COUNT = sizeof(NAME_CHARS) - 1;
static constexpr int CHOICE_COUNT    = 2 + NAME_CHAR_COUNT;
static constexpr int CHOICE_FIRST_CHAR = 2;  // 'A'

void DrinkEditor::start() {
  step_ = Step::Name;
  name_[0] = '\0';
  nameLength_ = 0;
  choice_ = CHOICE_FIRST_CHAR;
  pump_ = 0;
  for (int i = 0; i < PUMP_COUNT; i++) {
    amountsMl_[i] = 0;
  }
  lastTurnMs_ = 0;
}

void DrinkEditor::turn(int step) {
  switch (step_) {
    case Step::Name:
      // Wrap around the list in both directions.
      choice_ = (choice_ + step + CHOICE_COUNT) % CHOICE_COUNT;
      break;

    case Step::Amounts: {
      // Spinning the knob quickly takes bigger steps.
      uint32_t now = millis();
      float size = EDIT_ML_STEP;
      if (now - lastTurnMs_ < EDIT_FAST_TURN_MS) {
        size *= EDIT_FAST_MULTIPLIER;
      }
      lastTurnMs_ = now;

      float amount = amountsMl_[pump_] + step * size;
      amountsMl_[pump_] = constrain(amount, 0.0f, MAX_ML_PER_PUMP);
      break;
    }

    case Step::Confirm:
      break;
  }
}

DrinkEditor::Result DrinkEditor::press() {
  switch (step_) {
    case Step::Name:
      if (choice_ == CHOICE_DONE) {
        if (nameLength_ > 0) {  // a drink needs a name
          step_ = Step::Amounts;
          pump_ = 0;
        }
      } else if (choice_ == CHOICE_DELETE) {
        if (nameLength_ > 0) {
          name_[--nameLength_] = '\0';
        }
      } else if (nameLength_ < NAME_LIMIT) {
        name_[nameLength_++] = NAME_CHARS[choice_ - CHOICE_FIRST_CHAR];
        name_[nameLength_] = '\0';
      }
      break;

    case Step::Amounts:
      if (pump_ < PUMP_COUNT - 1) {
        pump_++;
      } else {
        step_ = Step::Confirm;
      }
      break;

    case Step::Confirm:
      if (hasAmounts()) {  // an all-zero drink can't be dispensed
        return Result::Save;
      }
      break;
  }
  return Result::Editing;
}

DrinkEditor::Result DrinkEditor::back() {
  switch (step_) {
    case Step::Name:
      return Result::Cancel;

    case Step::Amounts:
      if (pump_ > 0) {
        pump_--;
      } else {
        step_ = Step::Name;
      }
      break;

    case Step::Confirm:
      step_ = Step::Amounts;
      pump_ = PUMP_COUNT - 1;
      break;
  }
  return Result::Editing;
}

void DrinkEditor::draw(Display& display) const {
  switch (step_) {
    case Step::Name: {
      char choice[16];
      if (choice_ == CHOICE_DONE) {
        strcpy(choice, "[Done]");
      } else if (choice_ == CHOICE_DELETE) {
        strcpy(choice, "[Delete]");
      } else if (NAME_CHARS[choice_ - CHOICE_FIRST_CHAR] == ' ') {
        strcpy(choice, "[Space]");
      } else {
        choice[0] = NAME_CHARS[choice_ - CHOICE_FIRST_CHAR];
        choice[1] = '\0';
      }
      display.showNameEntry(name_, choice);
      break;
    }

    case Step::Amounts:
      display.showAmountEntry(pump_, amountsMl_[pump_]);
      break;

    case Step::Confirm:
      if (hasAmounts()) {
        display.showSaveConfirm(name_);
      } else {
        display.showNothingToSave();
      }
      break;
  }
}

bool DrinkEditor::hasAmounts() const {
  for (int i = 0; i < PUMP_COUNT; i++) {
    if (amountsMl_[i] > 0) {
      return true;
    }
  }
  return false;
}
