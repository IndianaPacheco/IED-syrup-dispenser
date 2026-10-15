/*
  PumpController.h
  ----------------
  Owns the eight pump driver outputs. Nothing else in the program
  should call digitalWrite() on a pump pin.

  Dispensing is non-blocking: start() switches the pumps on and
  update() must be called every loop() to switch each one off when
  its calibrated run time has elapsed.
*/

#pragma once

#include <Arduino.h>
#include "Config.h"

// Why start() did or did not begin dispensing.
enum class DispenseResult {
  Started,
  EmptyRecipe,     // every requested amount was 0 mL
  BadCalibration,  // a needed pump has a zero/invalid mL-per-second value
  DoseTooLong      // a pump would run longer than MAX_PUMP_RUN_MS
};

class PumpController {
public:
  // Configure the pump pins as outputs, all OFF.
  // Call this first in setup() so pumps never start at power-up.
  void begin();

  // Convert each volume (mL) into a run time and start the pumps.
  // All amounts are validated before any pump is switched on.
  DispenseResult start(const float amountsMl[PUMP_COUNT]);

  // Switch off pumps whose time is up. Call every loop().
  // Returns true while at least one pump is still running.
  bool update();

  // Immediately switch every pump off.
  // Does NOT remove power from the driver board itself.
  void stopAll();

  // Number of pumps currently switched on.
  int activeCount() const;

private:
  bool     running_[PUMP_COUNT]    = {};
  uint32_t durationMs_[PUMP_COUNT] = {};
  uint32_t startMs_                = 0;  // all pumps start together
};
