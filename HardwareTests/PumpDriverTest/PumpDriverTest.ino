/*
  PUMP DRIVER HARDWARE TEST
  -------------------------
  Switches the eight MOSFET pump drivers on one at a time, controlled
  from Serial Monitor.

  SAFETY:
    - First run this with PUMP POWER DISCONNECTED. Check each driver
      with its indicator LED or a multimeter on the gate/output.
    - Only one pump runs at a time, and every run has a time limit.
    - The stop button and the 's' command switch everything off.

  Commands (Serial Monitor at 115200 baud, line ending "Newline"):
    1..8    run that pump for 1 second
    a       run all pumps in order, 1 second each
    c1..c8  run that pump for 10 seconds (flow calibration)
    s       stop everything

  Calibration: run "cN" with the real syrup and tubing, measure the
  volume dispensed, and divide by 10. Put the result (mL/s) into
  PUMP_ML_PER_SECOND in SyrupDispenser/Config.h.

  Pass: each command turns on exactly the pump you asked for, and it
        turns off when the time is up or when you stop it.
*/

// Must match SyrupDispenser/Config.h
constexpr int PUMP_COUNT = 8;
constexpr int PUMP_PINS[PUMP_COUNT] = {
  13, 14, 16, 17, 25, 26, 27, 32
};
constexpr int STOP_BUTTON_PIN = 4;

constexpr uint32_t PULSE_MS       = 1000;
constexpr uint32_t CALIBRATION_MS = 10000;

int activePump = -1;        // -1 = nothing running
uint32_t pumpStartMs = 0;
uint32_t pumpDurationMs = 0;
bool sequenceMode = false;  // 'a' command: step through all pumps

char command[8];
int commandLength = 0;

void stopAll() {
  for (int i = 0; i < PUMP_COUNT; i++) {
    digitalWrite(PUMP_PINS[i], LOW);
  }
  activePump = -1;
  sequenceMode = false;
}

void runPump(int pump, uint32_t durationMs) {
  stopAll();
  activePump = pump;
  pumpStartMs = millis();
  pumpDurationMs = durationMs;
  digitalWrite(PUMP_PINS[pump], HIGH);
  Serial.printf("Pump %d ON (GPIO %d) for %lu ms\n",
                pump + 1, PUMP_PINS[pump], (unsigned long)durationMs);
}

void printHelp() {
  Serial.println("Commands: 1-8 = 1 s pulse, a = all in order,");
  Serial.println("          c1-c8 = 10 s calibration run, s = stop");
}

void handleCommand(const char* cmd) {
  if (strcmp(cmd, "s") == 0) {
    stopAll();
    Serial.println("All pumps OFF");
  } else if (strcmp(cmd, "a") == 0) {
    runPump(0, PULSE_MS);
    sequenceMode = true;
  } else if (cmd[0] >= '1' && cmd[0] <= '8' && cmd[1] == '\0') {
    runPump(cmd[0] - '1', PULSE_MS);
  } else if (cmd[0] == 'c' && cmd[1] >= '1' && cmd[1] <= '8' && cmd[2] == '\0') {
    runPump(cmd[1] - '1', CALIBRATION_MS);
  } else {
    Serial.printf("Unknown command: %s\n", cmd);
    printHelp();
  }
}

void readSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (commandLength > 0) {
        command[commandLength] = '\0';
        handleCommand(command);
        commandLength = 0;
      }
    } else if (commandLength < (int)sizeof(command) - 1) {
      command[commandLength++] = tolower(c);
    }
  }
}

void updatePump() {
  if (activePump < 0 || millis() - pumpStartMs < pumpDurationMs) {
    return;
  }

  digitalWrite(PUMP_PINS[activePump], LOW);
  Serial.printf("Pump %d OFF\n", activePump + 1);

  int next = activePump + 1;
  if (sequenceMode && next < PUMP_COUNT) {
    runPump(next, PULSE_MS);
    sequenceMode = true;  // runPump() clears it via stopAll()
  } else {
    if (sequenceMode) {
      Serial.println("Sequence complete");
    }
    activePump = -1;
    sequenceMode = false;
  }
}

void setup() {
  // Drive every pump pin LOW before anything else.
  for (int i = 0; i < PUMP_COUNT; i++) {
    digitalWrite(PUMP_PINS[i], LOW);
    pinMode(PUMP_PINS[i], OUTPUT);
    digitalWrite(PUMP_PINS[i], LOW);
  }
  pinMode(STOP_BUTTON_PIN, INPUT_PULLUP);

  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== PUMP DRIVER TEST ===");
  Serial.println("Test with pump power DISCONNECTED first!");
  printHelp();
}

void loop() {
  // Stop button: no debounce needed, any LOW reading stops everything.
  if (digitalRead(STOP_BUTTON_PIN) == LOW && activePump >= 0) {
    stopAll();
    Serial.println("STOP button: all pumps OFF");
  }

  readSerial();
  updatePump();
}
