/*
  SD CARD HARDWARE TEST
  ---------------------
  Checks the microSD card module on its own.

  What it does:
    1. Mounts the card and prints its type and size.
    2. Lists the files in the root folder.
    3. Writes a test file, reads it back, compares, then deletes it.
    4. Looks for the recipe file and prints how many lines it has.

  Pass: every step prints "OK" and the summary says "ALL PASSED".

  Open Serial Monitor at 115200 baud. Press the EN/RESET button on the
  ESP32 to run the test again.

  The card must be formatted FAT32.
*/

#include <SPI.h>
#include <SD.h>

// Must match SyrupDispenser/Config.h
constexpr int SD_CS_PIN   = 5;
constexpr int SD_SCK_PIN  = 18;
constexpr int SD_MISO_PIN = 19;
constexpr int SD_MOSI_PIN = 23;
constexpr uint32_t SD_SPI_HZ = 1000000;
constexpr const char* RECIPE_FILE = "/recipes.jsonl";

constexpr const char* TEST_FILE = "/_sdtest.txt";
constexpr const char* TEST_TEXT = "Syrup dispenser SD test 1234567890";

SPIClass sdSPI(VSPI);
int failures = 0;

void report(const char* step, bool ok) {
  Serial.printf("  %-28s %s\n", step, ok ? "OK" : "FAIL");
  if (!ok) {
    failures++;
  }
}

bool mountCard() {
  sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  bool ok = SD.begin(SD_CS_PIN, sdSPI, SD_SPI_HZ);
  report("Mount card", ok);
  if (!ok) {
    Serial.println("  -> Check wiring, 3.3V/5V power, card inserted, FAT32 format.");
    return false;
  }

  const char* type = "UNKNOWN";
  switch (SD.cardType()) {
    case CARD_MMC:  type = "MMC";  break;
    case CARD_SD:   type = "SDSC"; break;
    case CARD_SDHC: type = "SDHC"; break;
    case CARD_NONE: type = "NONE"; break;
    default: break;
  }
  Serial.printf("  Card type: %s\n", type);
  Serial.printf("  Card size: %llu MB\n", SD.cardSize() / (1024ULL * 1024ULL));
  return SD.cardType() != CARD_NONE;
}

void listRoot() {
  Serial.println("  Files in root:");
  File root = SD.open("/");
  if (!root) {
    report("Open root folder", false);
    return;
  }

  File entry = root.openNextFile();
  while (entry) {
    Serial.printf("    %s%s  (%u bytes)\n", entry.name(),
                  entry.isDirectory() ? "/" : "", (unsigned)entry.size());
    entry.close();
    entry = root.openNextFile();
  }
  root.close();
}

void writeReadTest() {
  File file = SD.open(TEST_FILE, FILE_WRITE);
  bool opened = file;
  report("Open test file for write", opened);
  if (!opened) {
    return;
  }
  size_t written = file.print(TEST_TEXT);
  file.close();
  report("Write test data", written == strlen(TEST_TEXT));

  file = SD.open(TEST_FILE, FILE_READ);
  String readBack = file ? file.readString() : "";
  file.close();
  report("Read back matches", readBack == TEST_TEXT);

  report("Delete test file", SD.remove(TEST_FILE));
}

void checkRecipeFile() {
  File file = SD.open(RECIPE_FILE, FILE_READ);
  if (!file) {
    Serial.printf("  %s not found (copy sd_card/recipes.jsonl to the card).\n",
                  RECIPE_FILE);
    failures++;
    return;
  }

  int lines = 0;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) {
      continue;
    }
    if (lines == 0) {
      Serial.printf("  First recipe: %s\n", line.c_str());
    }
    lines++;
  }
  file.close();

  Serial.printf("  Recipe lines: %d\n", lines);
  report("Recipe file present", lines > 0);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== SD CARD TEST ===");

  if (mountCard()) {
    listRoot();
    writeReadTest();
    checkRecipeFile();
  }

  Serial.println();
  if (failures == 0) {
    Serial.println("SD TEST: ALL PASSED");
  } else {
    Serial.printf("SD TEST: %d FAILURE(S)\n", failures);
  }
}

void loop() {}
