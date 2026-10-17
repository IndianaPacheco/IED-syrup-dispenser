#include "DrinkEditor.h"

// Name step: the letters come first, then the commands, all in one
// list the knob scrolls through.
static const char NAME_LETTERS[] = "abcdefghijklmnopqrstuvwxyz ";
static constexpr int LETTER_COUNT = sizeof(NAME_LETTERS) - 1;
static constexpr int NAME_DELETE  = LETTER_COUNT;
static constexpr int NAME_FINISH  = LETTER_COUNT + 1;
static constexpr int NAME_EXIT    = LETTER_COUNT + 2;
static constexpr int NAME_CHOICES = LETTER_COUNT + 3;

// Finish step menu.
static constexpr int FINISH_SAVE    = 0;
static constexpr int FINISH_EDIT    = 1;
static constexpr int FINISH_EXIT    = 2;
static constexpr int FINISH_CHOICES = 3;

void DrinkEditor::start() {
  step_ = Step::Name;
  name_[0] = '\0';
  nameLength_ = 0;
  choice_ = 0;  // 'a'
  pump_ = 0;
  for (int i = 0; i < PUMP_COUNT; i++) {
    amountsMl_[i] = 0;
  }
  lastTurnMs_ = 0;
}

void DrinkEditor::start(const char* name, const float amountsMl[PUMP_COUNT]) {
  start();

  strncpy(name_, name, NAME_LIMIT);
  name_[NAME_LIMIT] = '\0';
  nameLength_ = strlen(name_);
  for (int i = 0; i < PUMP_COUNT; i++) {
    amountsMl_[i] = amountsMl[i];
  }
  choice_ = NAME_FINISH;
}

void DrinkEditor::turn(int step) {
  switch (step_) {
    case Step::Name:
      // Wrap around the list in both directions.
      choice_ = (choice_ + step + NAME_CHOICES) % NAME_CHOICES;
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

    case Step::Finish:
      choice_ = (choice_ + step + FINISH_CHOICES) % FINISH_CHOICES;
      break;
  }
}

DrinkEditor::Result DrinkEditor::press() {
  switch (step_) {
    case Step::Name:
      if (choice_ < LETTER_COUNT) {
        bool leadingSpace = (nameLength_ == 0 && NAME_LETTERS[choice_] == ' ');
        if (nameLength_ < NAME_LIMIT && !leadingSpace) {
          // Capitalize the first letter of each word.
          char letter = NAME_LETTERS[choice_];
          bool startsWord = (nameLength_ == 0 || name_[nameLength_ - 1] == ' ');
          name_[nameLength_++] = startsWord ? toupper(letter) : letter;
          name_[nameLength_] = '\0';
        }
      } else if (choice_ == NAME_DELETE) {
        if (nameLength_ > 0) {
          name_[--nameLength_] = '\0';
        }
      } else if (choice_ == NAME_FINISH) {
        trimName();
        if (nameLength_ > 0) {  // a drink needs a name
          step_ = Step::Amounts;
          pump_ = 0;
        }
      } else if (choice_ == NAME_EXIT) {
        return Result::Cancel;
      }
      break;

    case Step::Amounts:
      if (pump_ < PUMP_COUNT - 1) {
        pump_++;
      } else {
        step_ = Step::Finish;
        choice_ = FINISH_SAVE;
      }
      break;

    case Step::Finish:
      if (choice_ == FINISH_SAVE) {
        if (hasAmounts()) {  // an all-zero drink can't be dispensed
          return Result::Save;
        }
      } else if (choice_ == FINISH_EDIT) {
        // Back to the name, keeping the amounts. The marker starts on
        // "Finish name" so one press moves on to the amounts again.
        step_ = Step::Name;
        choice_ = NAME_FINISH;
      } else if (choice_ == FINISH_EXIT) {
        return Result::Cancel;
      }
      break;
  }
  return Result::Editing;
}

void DrinkEditor::draw(Display& display) const {
  switch (step_) {
    case Step::Name:
      if (choice_ < LETTER_COUNT) {
        display.showLetterPicker(name_, NAME_LETTERS, choice_);
      } else {
        display.showNameCommands(name_, choice_ - NAME_DELETE);
      }
      break;

    case Step::Amounts:
      display.showAmountEntry(pump_, amountsMl_[pump_]);
      break;

    case Step::Finish:
      display.showFinishMenu(name_, hasAmounts(), choice_);
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

void DrinkEditor::trimName() {
  while (nameLength_ > 0 && name_[nameLength_ - 1] == ' ') {
    name_[--nameLength_] = '\0';
  }
}
