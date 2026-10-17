#include "RecipeManager.h"

#include <SPI.h>
#include <SD.h>

// The SD card gets its own SPI bus (VSPI). Only this file uses it.
static SPIClass sdSPI(VSPI);

bool RecipeManager::beginCard() {
  sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  if (!SD.begin(SD_CS_PIN, sdSPI, SD_SPI_HZ)) {
    Serial.println("SD card initialization failed.");
    Serial.println("Check wiring, power, and FAT32 format.");
    return false;
  }
  return true;
}

bool RecipeManager::buildIndex() {
  count_ = 0;
  maxId_ = 0;

  // No file yet: start with an empty list. addRecipe() creates it.
  if (!SD.exists(RECIPE_FILE)) {
    Serial.print(RECIPE_FILE);
    Serial.println(" not found; starting with no recipes.");
    return true;
  }

  File file = SD.open(RECIPE_FILE, FILE_READ);
  if (!file) {
    Serial.print("Could not open ");
    Serial.println(RECIPE_FILE);
    return false;
  }

  while (file.available()) {
    // Remember where this line starts so we can jump back to it later.
    uint32_t offset = file.position();

    String line = file.readStringUntil('\n');
    line.trim();

    if (line.length() == 0) {
      continue;  // blank line
    }

    if (count_ >= MAX_RECIPES) {
      Serial.println("Recipe index limit reached; extra recipes ignored.");
      break;
    }

    // readRecipeAt() can only hold MAX_JSON_LINE bytes, so a longer
    // line would index fine here but fail to load later. Reject it now.
    if (line.length() >= MAX_JSON_LINE) {
      Serial.println("Skipping recipe: line too long.");
      continue;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, line);
    if (error) {
      Serial.print("Skipping invalid JSON line: ");
      Serial.println(error.c_str());
      continue;
    }

    // Check the recipe has every field we need.
    if (!doc["id"].is<int>() ||
        !doc["name"].is<const char*>() ||
        !doc["amounts_ml"].is<JsonArrayConst>()) {
      Serial.println("Skipping malformed recipe.");
      continue;
    }

    if (doc["amounts_ml"].as<JsonArrayConst>().size() != PUMP_COUNT) {
      Serial.println("Skipping recipe with wrong pump count.");
      continue;
    }

    IndexEntry& entry = index_[count_];
    entry.fileOffset = offset;
    entry.id = doc["id"].as<int>();
    if (entry.id > maxId_) {
      maxId_ = entry.id;
    }

    // Copy the name, truncating it safely if it is too long.
    strncpy(entry.name, doc["name"].as<const char*>(), MAX_NAME_LENGTH - 1);
    entry.name[MAX_NAME_LENGTH - 1] = '\0';

    count_++;
  }

  file.close();

  Serial.print("Recipes indexed: ");
  Serial.println(count_);

  return true;
}

bool RecipeManager::addRecipe(const char* name, const float amountsMl[PUMP_COUNT]) {
  if (isFull()) {
    Serial.println("Cannot add recipe: index is full.");
    return false;
  }

  char line[MAX_JSON_LINE];
  size_t length = buildLine(maxId_ + 1, name, amountsMl, line);
  if (length == 0) {
    Serial.println("Cannot add recipe: line too long.");
    return false;
  }

  // If the last line has no newline (common when the file was edited
  // by hand), add one so the new recipe starts on its own line.
  bool needsNewline = false;
  File existing = SD.open(RECIPE_FILE, FILE_READ);
  if (existing) {
    size_t size = existing.size();
    if (size > 0 && existing.seek(size - 1)) {
      needsNewline = existing.read() != '\n';
    }
    existing.close();
  }

  File file = SD.open(RECIPE_FILE, FILE_APPEND);  // creates if missing
  if (!file) {
    Serial.println("Cannot add recipe: file did not open for writing.");
    return false;
  }

  bool ok = true;
  if (needsNewline) {
    ok = file.print('\n') == 1;
  }
  ok = ok && file.write((const uint8_t*)line, length) == length;
  ok = ok && file.print('\n') == 1;
  file.close();

  if (!ok) {
    Serial.println("Cannot add recipe: write failed (card full?).");
    return false;
  }

  Serial.print("Saved recipe: ");
  Serial.println(line);

  return buildIndex();
}

bool RecipeManager::replaceRecipe(int index, const char* name,
                                  const float amountsMl[PUMP_COUNT]) {
  if (index < 0 || index >= count_) {
    return false;
  }

  char line[MAX_JSON_LINE];
  if (buildLine(index_[index].id, name, amountsMl, line) == 0) {
    Serial.println("Cannot replace recipe: line too long.");
    return false;
  }

  Serial.print("Replacing recipe with: ");
  Serial.println(line);
  return rewriteRecipe(index, line);
}

bool RecipeManager::deleteRecipe(int index) {
  if (index < 0 || index >= count_) {
    return false;
  }

  Serial.print("Deleting recipe: ");
  Serial.println(index_[index].name);
  return rewriteRecipe(index, nullptr);
}

const char* RecipeManager::name(int index) const {
  if (index < 0 || index >= count_) {
    return "";
  }
  return index_[index].name;
}

bool RecipeManager::loadAmounts(int index, float amountsMl[PUMP_COUNT]) const {
  if (index < 0 || index >= count_) {
    return false;
  }

  JsonDocument doc;
  if (!readRecipeAt(index_[index].fileOffset, doc)) {
    Serial.println("Failed to load selected recipe.");
    return false;
  }

  JsonArrayConst amounts = doc["amounts_ml"];
  if (amounts.size() != PUMP_COUNT) {
    return false;
  }

  for (int i = 0; i < PUMP_COUNT; i++) {
    if (!amounts[i].is<float>()) {
      return false;  // not a number
    }

    float amount = amounts[i].as<float>();

    // Reject negative, excessive, or invalid amounts.
    if (!isfinite(amount) || amount < 0 || amount > MAX_ML_PER_PUMP) {
      return false;
    }

    amountsMl[i] = amount;
  }

  return true;
}

bool RecipeManager::readRecipeAt(uint32_t offset, JsonDocument& doc) const {
  File file = SD.open(RECIPE_FILE, FILE_READ);
  if (!file) {
    return false;
  }

  if (!file.seek(offset)) {
    file.close();
    return false;
  }

  // Read one line into a fixed-size buffer (no heap allocation).
  char line[MAX_JSON_LINE];
  size_t length = file.readBytesUntil('\n', line, sizeof(line) - 1);
  line[length] = '\0';
  file.close();

  // Strip trailing '\r' (Windows line endings) and spaces.
  while (length > 0 && (line[length - 1] == '\r' || line[length - 1] == ' ')) {
    line[--length] = '\0';
  }

  if (length == 0) {
    return false;
  }

  return !deserializeJson(doc, line);
}

size_t RecipeManager::buildLine(int id, const char* name,
                                const float amountsMl[PUMP_COUNT],
                                char line[MAX_JSON_LINE]) const {
  // Build the line with ArduinoJson so the name is escaped properly.
  JsonDocument doc;
  doc["id"] = id;
  doc["name"] = name;
  JsonArray amounts = doc["amounts_ml"].to<JsonArray>();
  for (int i = 0; i < PUMP_COUNT; i++) {
    amounts.add(amountsMl[i]);
  }

  size_t length = serializeJson(doc, line, MAX_JSON_LINE);
  if (length == 0 || length >= MAX_JSON_LINE - 1) {
    return 0;
  }
  return length;
}

bool RecipeManager::rewriteRecipe(int index, const char* replacement) {
  static constexpr const char* TEMP_FILE = "/recipes.tmp";

  File source = SD.open(RECIPE_FILE, FILE_READ);
  if (!source) {
    Serial.println("Cannot rewrite recipes: file did not open.");
    return false;
  }

  SD.remove(TEMP_FILE);  // leftover from an earlier failed rewrite
  File temp = SD.open(TEMP_FILE, FILE_WRITE);
  if (!temp) {
    source.close();
    Serial.println("Cannot rewrite recipes: temp file did not open.");
    return false;
  }

  // Copy every line as-is (blank and invalid lines too), except the
  // one that starts where recipe 'index' does.
  uint32_t target = index_[index].fileOffset;
  bool ok = true;
  while (ok && source.available()) {
    uint32_t offset = source.position();
    String line = source.readStringUntil('\n');

    if (offset == target) {
      if (replacement != nullptr) {
        ok = temp.print(replacement) == strlen(replacement) &&
             temp.print('\n') == 1;
      }
    } else {
      // readStringUntil() drops the '\n'; keep any '\r' as it was.
      ok = temp.print(line) == line.length() && temp.print('\n') == 1;
    }
  }
  source.close();
  temp.close();

  if (!ok) {
    SD.remove(TEMP_FILE);
    Serial.println("Cannot rewrite recipes: write failed (card full?).");
    return false;
  }

  // Swap the new file in. The original is only removed once the
  // full copy is safely on the card.
  if (!SD.remove(RECIPE_FILE) || !SD.rename(TEMP_FILE, RECIPE_FILE)) {
    Serial.print("Cannot rewrite recipes: rename failed. Data is in ");
    Serial.println(TEMP_FILE);
    return false;
  }

  return buildIndex();
}
