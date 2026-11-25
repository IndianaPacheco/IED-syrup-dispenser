/*
  PumpController.h
  ----------------
  Owns the eight pump driver outputs. Nothing else in the program
  should write to a pump pin. Running pumps are driven with PWM so
  they never get more than PUMP_MAX_VOLTS on average (see Config.h).

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
  SpeedZero,       // the pump speed setting is 0%
  DriverError      // the PWM output could not be switched on
};

class PumpController {
public:
  // Configure the pump pins as outputs, all OFF.
  // Call this first in setup() so pumps never start at power-up.
  void begin();

  // Convert each volume (mL) into a run time and start the pumps.
  // speedPercent (0-100) is the share of PUMP_MAX_VOLTS to run at;
  // slower speeds run proportionally longer to pour the same volume.
  // All amounts are validated before any pump is switched on.
  DispenseResult start(const float amountsMl[PUMP_COUNT], int speedPercent);

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
