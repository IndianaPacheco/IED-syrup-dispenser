#include "TextEntry.h"

// Every printable character, split into pages of at most 27 so a
// page plus its page switch fits on three LCD rows.
static const char* const PAGES[TextEntry::PAGE_COUNT] = {
  "abcdefghijklmnopqrstuvwxyz",
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
  "0123456789 .-_@!#$%&*?+=",
  "^()/:;,'\"~`<>[]{}|\\",
};

// The page switch shows where it goes next.
static const char* const PAGE_NAMES[TextEntry::PAGE_COUNT] = {
  "abc", "ABC", "123", "#$%"
};

void TextEntry::start(const char* label, const char* initial, int maxLength,
                      int minLength, bool allowEmpty, Page firstPage) {
  label_ = label;
  maxLength_ = min(maxLength, TEXT_LIMIT);
  minLength_ = minLength;
  allowEmpty_ = allowEmpty;
  page_ = firstPage;
  tooShort_ = false;

  strncpy(text_, initial, maxLength_);
  text_[maxLength_] = '\0';
  length_ = strlen(text_);

  // Existing text is probably right, so start on Done; otherwise on
  // the first character.
  choice_ = (length_ > 0) ? doneChoice() : 0;
}

int TextEntry::pageLength() const {
  return strlen(PAGES[page_]);
}

void TextEntry::turn(int step) {
  tooShort_ = false;
  choice_ = (choice_ + step + choiceCount()) % choiceCount();
}

TextEntry::Result TextEntry::press() {
  tooShort_ = false;

  if (choice_ < pageLength()) {
    if (length_ < maxLength_) {
      text_[length_++] = PAGES[page_][choice_];
      text_[length_] = '\0';
    }
  } else if (choice_ == switchChoice()) {
    // Stay on the switch, so pressing again keeps flipping pages.
    page_ = (page_ + 1) % PAGE_COUNT;
    choice_ = switchChoice();
  } else if (choice_ == deleteChoice()) {
    if (length_ > 0) {
      text_[--length_] = '\0';
    }
  } else if (choice_ == doneChoice()) {
    bool longEnough = length_ >= minLength_ || (length_ == 0 && allowEmpty_);
    if (longEnough) {
      return Result::Done;
    }
    tooShort_ = true;
  } else if (choice_ == cancelChoice()) {
    return Result::Cancel;
  }
  return Result::Editing;
}

void TextEntry::draw(Display& display) const {
  if (choice_ <= switchChoice()) {
    int next = (page_ + 1) % PAGE_COUNT;
    display.showTextPicker(label_, text_, PAGES[page_], choice_,
                           PAGE_NAMES[next]);
  } else {
    char warning[LCD_COLUMNS + 1] = "";
    if (tooShort_) {
      snprintf(warning, sizeof(warning), "Need %d+ characters", minLength_);
    }
    display.showTextCommands(label_, text_, choice_ - deleteChoice(), warning);
  }
}
