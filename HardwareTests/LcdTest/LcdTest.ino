/*
  LCD HARDWARE TEST
  -----------------
  Checks the 20x4 I2C LCD on its own, without the rest of the dispenser.

  What it does:
    1. Scans the I2C bus and prints every device address it finds.
       If the LCD shows up at an address other than LCD_I2C_ADDRESS,
       change it here AND in SyrupDispenser/Config.h.
    2. Blinks the backlight 3 times.
    3. Fills the screen with solid blocks so you can check every
       pixel and adjust the contrast pot on the back of the LCD.
    4. Prints a test pattern, then counts up once per second forever.

  Pass: backlight blinks, all 80 blocks are solid, text is readable,
        and the counter keeps going up.

  Open Serial Monitor at 115200 baud to see the scan results.

  Library: LiquidCrystal I2C (Frank de Brabander)
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Must match SyrupDispenser/Config.h
constexpr uint8_t LCD_I2C_ADDRESS = 0x27;
constexpr int LCD_COLUMNS = 20;
constexpr int LCD_ROWS    = 4;
constexpr int LCD_SDA_PIN = 21;
constexpr int LCD_SCL_PIN = 22;

LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS);

// Returns true if a device answered at LCD_I2C_ADDRESS.
bool scanI2C() {
  Serial.println("Scanning I2C bus...");
  int found = 0;
  bool lcdFound = false;

  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Device found at 0x%02X\n", address);
      found++;
      if (address == LCD_I2C_ADDRESS) {
        lcdFound = true;
      }
    }
  }

  if (found == 0) {
    Serial.println("  No I2C devices found. Check SDA/SCL wiring and 5V/GND.");
  }
  return lcdFound;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== LCD TEST ===");

  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);

  if (!scanI2C()) {
    Serial.printf("FAIL: nothing at 0x%02X. Update LCD_I2C_ADDRESS to the "
                  "address found above.\n", LCD_I2C_ADDRESS);
    return;
  }
  Serial.println("LCD responded. Watch the screen.");

  lcd.init();

  Serial.println("Step 1: blinking backlight");
  for (int i = 0; i < 3; i++) {
    lcd.backlight();
    delay(400);
    lcd.noBacklight();
    delay(400);
  }
  lcd.backlight();

  Serial.println("Step 2: all pixels on (adjust contrast pot if needed)");
  for (int row = 0; row < LCD_ROWS; row++) {
    lcd.setCursor(0, row);
    for (int col = 0; col < LCD_COLUMNS; col++) {
      lcd.write(0xFF);  // solid block character
    }
  }
  delay(3000);

  Serial.println("Step 3: text pattern");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("0123456789ABCDEFGHIJ");
  lcd.setCursor(0, 1);
  lcd.print("Row 2");
  lcd.setCursor(0, 2);
  lcd.print("Row 3");
  lcd.setCursor(0, 3);
  lcd.print("LCD TEST OK");
  delay(3000);

  Serial.println("Step 4: counter (should tick once per second)");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Uptime (s):");
}

void loop() {
  static uint32_t lastUpdate = 0;
  static uint32_t seconds = 0;

  if (millis() - lastUpdate >= 1000) {
    lastUpdate = millis();
    lcd.setCursor(0, 1);
    lcd.printf("%-20lu", (unsigned long)seconds++);
  }
}
