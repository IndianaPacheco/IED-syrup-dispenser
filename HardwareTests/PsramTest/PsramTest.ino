/*
  PSRAM / MODULE TYPE TEST

  Tells you whether the ESP32 board has a WROOM or WROVER module.
  WROVER modules have PSRAM wired to GPIO 16 and 17, which the rotary
  encoder (CLK and DT) uses, so this matters for the knob.

  Before uploading: Tools -> PSRAM -> Enabled (if your board menu has it).
  Open Serial Monitor at 115200 baud.

  Result:
    PSRAM size 0         -> WROOM. GPIO 16/17 are free. Nothing to change.
    PSRAM size ~4-8 MB   -> WROVER. Move the encoder off GPIO 16/17.
*/

void setup() {
  Serial.begin(115200);
}

// Repeats every 2 s so you see it even if Serial Monitor opens late.
void loop() {
  uint32_t psram = ESP.getPsramSize();
  Serial.printf("PSRAM size: %u bytes\n", psram);
  if (psram == 0) {
    Serial.println("No PSRAM -> WROOM module. GPIO 16/17 are OK for the encoder.");
  } else {
    Serial.println("PSRAM found -> WROVER module. Do NOT use GPIO 16/17 for the encoder.");
  }
  delay(2000);
}
