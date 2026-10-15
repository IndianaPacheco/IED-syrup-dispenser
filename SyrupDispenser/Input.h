/*
  Input.h
  -------
  Debounced push buttons and the rotary encoder.

  Both are polled: call their read functions once every loop().
  The main loop never blocks (no delay()), so polling is fast enough.
*/

#pragma once

#include <Arduino.h>

// A push button wired between its pin and GND (pressed = LOW).
class DebouncedButton {
public:
  // mode is INPUT_PULLUP, or INPUT if the pin has an external
  // pull-up resistor (required on GPIO 34-39).
  DebouncedButton(int pin, uint8_t mode);

  void begin();

  // True exactly once per press, after the signal has been
  // steady for DEBOUNCE_MS.
  bool wasPressed();

private:
  int      pin_;
  uint8_t  mode_;
  bool     lastRaw_    = HIGH;  // last raw reading
  bool     stable_     = HIGH;  // last debounced state
  uint32_t lastChange_ = 0;     // when the raw reading last changed
};

// A KY-040 style rotary encoder (the knob only; its push switch is
// a separate DebouncedButton).
class RotaryEncoder {
public:
  RotaryEncoder(int clkPin, int dtPin);

  void begin();

  // +1 for one click clockwise, -1 for counter-clockwise, 0 if the
  // knob has not moved since the last call.
  int readStep();

private:
  int      clkPin_;
  int      dtPin_;
  int      lastClk_  = HIGH;
  uint32_t lastStep_ = 0;
};
