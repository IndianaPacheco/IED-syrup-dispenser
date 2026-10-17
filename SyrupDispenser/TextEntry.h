/*
  TextEntry.h
  -----------
  Typing any text (WiFi passwords, network names, the web PIN) with
  only the knob. Like the drink name picker, but with four pages of
  characters so passwords can use capitals, numbers, and symbols:

    abc  ->  ABC  ->  123 (digits, space, common symbols)  ->  #$%

  Turn to move the ">" marker, press to add the marked character.
  After the last character on a page comes the page switch (shows
  the next page's name, press to flip), then Delete letter / Done /
  Cancel. Turning back from the first character wraps to Cancel.

  The text is shown as typed (not hidden), since only staff can see
  the machine's screen.
*/

#pragma once

#include <Arduino.h>
#include "Display.h"

class TextEntry {
public:
  enum class Result {
    Editing,  // still typing
    Done,     // accepted; read text()
    Cancel    // backed out; ignore text()
  };

  // Which character page to show first.
  enum Page { LOWER, UPPER, DIGITS, SYMBOLS, PAGE_COUNT };

  // label:     shown before the text on the top row, e.g. "Pass:"
  // initial:   text to start with ("" for none)
  // maxLength: longest text allowed
  // minLength: Done is refused for shorter text, except that empty
  //            text is allowed when allowEmpty is true
  void start(const char* label, const char* initial, int maxLength,
             int minLength, bool allowEmpty, Page firstPage = LOWER);

  void turn(int step);
  Result press();
  void draw(Display& display) const;

  const char* text() const { return text_; }

private:
  // Number of characters on the current page.
  int pageLength() const;

  // Marker positions after the characters.
  int switchChoice() const { return pageLength(); }
  int deleteChoice() const { return pageLength() + 1; }
  int doneChoice() const   { return pageLength() + 2; }
  int cancelChoice() const { return pageLength() + 3; }
  int choiceCount() const  { return pageLength() + 4; }

  static constexpr int TEXT_LIMIT = 63;  // longest WiFi password

  const char* label_ = "";
  char text_[TEXT_LIMIT + 1] = "";
  int  length_ = 0;
  int  maxLength_ = TEXT_LIMIT;
  int  minLength_ = 0;
  bool allowEmpty_ = true;
  int  page_ = LOWER;
  int  choice_ = 0;
  bool tooShort_ = false;  // Done was refused; say why until the next input
};
