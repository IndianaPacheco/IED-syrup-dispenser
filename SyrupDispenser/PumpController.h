/*
  PumpController.h
  ----------------
  Owns the eight pump driver outputs. Nothing else in the program
  should write to a pump pin. Pouring uses PWM at an effective
  PUMP_POUR_VOLTS; only startCleaning() uses the full supply voltage
  (see Config.h).

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
  DoseTooLong,     // a pump would run longer than MAX_PUMP_RUN_MS
  DriverError      // the PWM output could not be switched on
};

class PumpController {
public:
  // Configure the pump pins as outputs, all OFF.
  // Call this first in setup() so pumps never start at power-up.
  void begin();

  // Convert each volume (mL) into a run time and start the pumps.
  // Pumps run at an effective PUMP_POUR_VOLTS.
  // All amounts are validated before any pump is switched on.
  DispenseResult start(const float amountsMl[PUMP_COUNT]);

  // Run every pump at the full supply voltage for CLEAN_RUN_MS.
  // Returns false (all pumps off) if a pump can't be switched on.
  bool startCleaning();

  // Seconds until the longest-running pump switches off, rounded up.
  int secondsLeft() const;

  // Switch off pumps whose time is up. Call every loop().
  // Returns true while at least one pump is still running.
  bool update();

  // Immediately switch every pump off.
  // Does NOT remove power from the driver board itself.
  void stopAll();

  // Number of pumps currently switched on.
  int activeCount() const;

private:
  // Switch on every pump with a non-zero run time, all at 'duty'.
  // On failure everything is switched off again.
  bool runPumps(const uint32_t durations[PUMP_COUNT], uint32_t duty);

  bool     running_[PUMP_COUNT]    = {};
  uint32_t durationMs_[PUMP_COUNT] = {};
  uint32_t startMs_                = 0;  // all pumps start together
};
