/*
  ENCODER + BUTTON HARDWARE TEST
  ------------------------------
  Checks the KY-040 rotary encoder, its push switch, and the stop button.

  What it does:
    - At startup, prints the idle level of every input pin. All should
      read HIGH. A LOW at idle usually means a missing pull-up resistor
      (GPIO 34 and 35 rely on the KY-040's onboard 10 kOhm pull-ups,
      so check that the module's + pin is wired to 3.3 V).
    - Prints the knob position every time it moves, and a message on
      every encoder press and stop-button press.
    - Type 'r' in Serial Monitor to print the raw pin levels again.

  Pass: one click clockwise = position +1, one click counter-clockwise
        = -1 (exactly one count per detent, no skipped or double counts),
        and each press prints exactly once.

  If the direction is backwards, swap the CLK and DT wires.

  Open Serial Monitor at 115200 baud.
*/

// Must match SyrupDispenser/Config.h
constexpr int ENCODER_CLK_PIN = 33;
constexpr int ENCODER_DT_PIN  = 34;
constexpr int ENCODER_SW_PIN  = 35;
constexpr int STOP_BUTTON_PIN = 4;
constexpr uint32_t DEBOUNCE_MS = 35;

// Same detection method as SyrupDispenser/Input.cpp, so what you see
// here is how the real program will behave.
constexpr uint32_t ENCODER_MIN_STEP_MS = 3;

struct Button {
  int pin;
  bool lastRaw;
  bool stable;
  uint32_t lastChange;
};

Button selectButton = {ENCODER_SW_PIN, HIGH, HIGH, 0};
Button stopButton   = {STOP_BUTTON_PIN, HIGH, HIGH, 0};

int lastClk = HIGH;
uint32_t lastStep = 0;
long position = 0;
int selectPresses = 0;
int stopPresses = 0;

void printPinLevels() {
  Serial.println("Raw pin levels (should all be HIGH when idle):");
  Serial.printf("  CLK  GPIO %2d: %s\n", ENCODER_CLK_PIN,
                digitalRead(ENCODER_CLK_PIN) ? "HIGH" : "LOW");
  Serial.printf("  DT   GPIO %2d: %s\n", ENCODER_DT_PIN,
                digitalRead(ENCODER_DT_PIN) ? "HIGH" : "LOW  <- check pull-up");
  Serial.printf("  SW   GPIO %2d: %s\n", ENCODER_SW_PIN,
                digitalRead(ENCODER_SW_PIN) ? "HIGH" : "LOW  <- check pull-up");
  Serial.printf("  STOP GPIO %2d: %s\n", STOP_BUTTON_PIN,
                digitalRead(STOP_BUTTON_PIN) ? "HIGH" : "LOW");
}

// True once per press, after DEBOUNCE_MS of steady signal.
bool wasPressed(Button& b) {
  bool raw = digitalRead(b.pin);
  uint32_t now = millis();

  if (raw != b.lastRaw) {
    b.lastRaw = raw;
    b.lastChange = now;
  }

  if (now - b.lastChange >= DEBOUNCE_MS && raw != b.stable) {
    b.stable = raw;
    return b.stable == LOW;
  }
  return false;
}

int readStep() {
  int clk = digitalRead(ENCODER_CLK_PIN);
  int step = 0;

  if (lastClk == HIGH && clk == LOW) {
    uint32_t now = millis();
    if (now - lastStep >= ENCODER_MIN_STEP_MS) {
      lastStep = now;
      step = (digitalRead(ENCODER_DT_PIN) == HIGH) ? +1 : -1;
    }
  }

  lastClk = clk;
  return step;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== ENCODER + BUTTON TEST ===");

  pinMode(ENCODER_CLK_PIN, INPUT_PULLUP);
  pinMode(ENCODER_DT_PIN, INPUT);   // input-only pin, KY-040 pull-up
  pinMode(ENCODER_SW_PIN, INPUT);   // input-only pin, KY-040 pull-up
  pinMode(STOP_BUTTON_PIN, INPUT_PULLUP);
  delay(10);

  lastClk = digitalRead(ENCODER_CLK_PIN);
  selectButton.lastRaw = selectButton.stable = digitalRead(ENCODER_SW_PIN);
  stopButton.lastRaw = stopButton.stable = digitalRead(STOP_BUTTON_PIN);

  printPinLevels();
  Serial.println("Turn the knob and press the buttons. Type 'r' for pin levels.");
}

void loop() {
  int step = readStep();
  if (step != 0) {
    position += step;
    Serial.printf("Knob %s  position = %ld\n", step > 0 ? "CW " : "CCW", position);
  }

  if (wasPressed(selectButton)) {
    Serial.printf("Encoder button pressed (%d)\n", ++selectPresses);
  }

  if (wasPressed(stopButton)) {
    Serial.printf("STOP button pressed (%d)\n", ++stopPresses);
  }

  if (Serial.available() && Serial.read() == 'r') {
    printPinLevels();
  }
}
