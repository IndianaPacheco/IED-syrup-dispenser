#include "PumpController.h"

void PumpController::begin() {
  for (int i = 0; i < PUMP_COUNT; i++) {
    // Write LOW before switching to OUTPUT so the pin never
    // briefly drives HIGH while being configured.
    digitalWrite(PUMP_PINS[i], LOW);
    pinMode(PUMP_PINS[i], OUTPUT);
    digitalWrite(PUMP_PINS[i], LOW);
  }
  stopAll();
}

DispenseResult PumpController::start(const float amountsMl[PUMP_COUNT]) {
  stopAll();

  // Pass 1: work out every run time. If anything is wrong we return
  // before a single pump has been switched on.
  uint32_t durations[PUMP_COUNT] = {};
  bool anyRequested = false;

  for (int i = 0; i < PUMP_COUNT; i++) {
    if (amountsMl[i] <= 0) {
      continue;  // this syrup is not in the recipe
    }

    float rate = PUMP_ML_PER_SECOND[i];
    if (!isfinite(rate) || rate <= 0) {
      return DispenseResult::BadCalibration;
    }

    // time (ms) = volume (mL) / rate (mL/s) * 1000
    float ms = (amountsMl[i] / rate) * 1000.0f;
    if (!isfinite(ms) || ms > MAX_PUMP_RUN_MS) {
      return DispenseResult::DoseTooLong;
    }

    durations[i] = (uint32_t)ms;
    anyRequested = true;
  }

  if (!anyRequested) {
    return DispenseResult::EmptyRecipe;
  }

  // Pass 2: everything is valid, so switch the pumps on together.
  // Timed dosing is approximate; a flow sensor or scale would be
  // needed to verify the volume actually dispensed.
  startMs_ = millis();
  for (int i = 0; i < PUMP_COUNT; i++) {
    durationMs_[i] = durations[i];
    if (durations[i] > 0) {
      running_[i] = true;
      digitalWrite(PUMP_PINS[i], HIGH);
    }
  }

  return DispenseResult::Started;
}

bool PumpController::update() {
  // Unsigned subtraction keeps this correct across the ~49-day
  // millis() rollover.
  uint32_t elapsed = millis() - startMs_;
  bool anyRunning = false;

  for (int i = 0; i < PUMP_COUNT; i++) {
    if (!running_[i]) {
      continue;
    }

    if (elapsed >= durationMs_[i]) {
      digitalWrite(PUMP_PINS[i], LOW);
      running_[i] = false;
    } else {
      anyRunning = true;
    }
  }

  return anyRunning;
}

void PumpController::stopAll() {
  for (int i = 0; i < PUMP_COUNT; i++) {
    digitalWrite(PUMP_PINS[i], LOW);
    running_[i] = false;
  }
}

int PumpController::activeCount() const {
  int count = 0;
  for (int i = 0; i < PUMP_COUNT; i++) {
    if (running_[i]) {
      count++;
    }
  }
  return count;
}
