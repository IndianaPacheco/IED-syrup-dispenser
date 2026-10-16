#include "Input.h"
#include "Config.h"

// Ignore encoder edges closer together than this (contact bounce).
static constexpr uint32_t ENCODER_MIN_STEP_MS = 3;

// ============================================================
// DebouncedButton
// ============================================================

DebouncedButton::DebouncedButton(int pin, uint8_t mode)
    : pin_(pin), mode_(mode) {}

void DebouncedButton::begin() {
  pinMode(pin_, mode_);

  // Start from the current state so a button held at power-up
  // doesn't register as a press.
  lastRaw_ = digitalRead(pin_);
  stable_ = lastRaw_;
  lastChange_ = millis();
}

bool DebouncedButton::wasPressed() {
  bool raw = digitalRead(pin_);
  uint32_t now = millis();

  // Any change restarts the debounce timer.
  if (raw != lastRaw_) {
    lastRaw_ = raw;
    lastChange_ = now;
  }

  // Accept the new state only once it has been steady long enough.
  if (now - lastChange_ >= DEBOUNCE_MS && raw != stable_) {
    stable_ = raw;
    return stable_ == LOW;  // report presses, not releases
  }

  return false;
}

// ============================================================
// RotaryEncoder
// ============================================================

RotaryEncoder::RotaryEncoder(int clkPin, int dtPin)
    : clkPin_(clkPin), dtPin_(dtPin) {}

void RotaryEncoder::begin() {
  pinMode(clkPin_, INPUT_PULLUP);
  pinMode(dtPin_, INPUT);  // GPIO 34: uses the KY-040's onboard pull-up
  lastClk_ = digitalRead(clkPin_);
}

int RotaryEncoder::readStep() {
  int clk = digitalRead(clkPin_);
  int step = 0;

  // One falling edge on CLK = one detent. DT at that moment
  // tells us the direction.
  if (lastClk_ == HIGH && clk == LOW) {
    uint32_t now = millis();
    if (now - lastStep_ >= ENCODER_MIN_STEP_MS) {
      lastStep_ = now;
      step = (digitalRead(dtPin_) == HIGH) ? +1 : -1;
    }
  }

  lastClk_ = clk;
  return step;
}
