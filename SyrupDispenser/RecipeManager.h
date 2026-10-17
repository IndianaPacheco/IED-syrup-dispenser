/*
  RecipeManager.h
  ---------------
  Reads drink recipes from the microSD card.

  The recipe file holds one JSON object per line, for example:
    {"id":1,"name":"Vanilla Latte","amounts_ml":[10,0,0,0,0,0,0,0]}

  To support thousands of recipes without running out of RAM, only
  each recipe's name and byte position in the file are kept in memory.
  The full recipe is re-read from the card when it is selected.
*/

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "Config.h"

class RecipeManager {
public:
  // Start the SPI bus and mount the SD card.
  // Returns false if the card is missing, unformatted, or miswired.
  bool beginCard();

  // Scan the recipe file and build the in-memory index.
  // Invalid lines are skipped (and reported on Serial).
  // A missing file counts as zero recipes. Returns false only if
  // the file exists but cannot be opened.
  bool buildIndex();

  // Number of valid recipes found by buildIndex().
  int count() const { return count_; }

  // True when no more recipes can be added.
  bool isFull() const { return count_ >= MAX_RECIPES; }

  // Append a new recipe to the end of the recipe file, giving it
  // the next free id, then rebuild the index. The new recipe is
  // the last one (index count() - 1). Returns false on any error.
  bool addRecipe(const char* name, const float amountsMl[PUMP_COUNT]);

  // Overwrite recipe 'index' with a new name and amounts, keeping its
  // id and its place in the file. Returns false on any error.
  bool replaceRecipe(int index, const char* name,
                     const float amountsMl[PUMP_COUNT]);

  // Remove recipe 'index' from the file. Returns false on any error.
  bool deleteRecipe(int index);

  // Display name of recipe 'index' (0-based).
  const char* name(int index) const;

  // Read recipe 'index' from the card and copy its eight volumes
  // (mL) into amountsMl. Returns false if the data is missing or
  // any volume is negative, too large, or not a number.
  bool loadAmounts(int index, float amountsMl[PUMP_COUNT]) const;

private:
  // What we remember about each recipe without loading all of it.
  struct IndexEntry {
    uint32_t fileOffset;           // where its line starts in the file
    int      id;
    char     name[MAX_NAME_LENGTH];
  };

  // Read and parse the single line that starts at 'offset'.
  bool readRecipeAt(uint32_t offset, JsonDocument& doc) const;

  // Write one recipe as a JSON line (no newline) into 'line'.
  // Returns its length, or 0 if it doesn't fit.
  size_t buildLine(int id, const char* name, const float amountsMl[PUMP_COUNT],
                   char line[MAX_JSON_LINE]) const;

  // Copy the recipe file to a temp file, swapping recipe 'index' for
  // 'replacement' (or dropping it if replacement is nullptr), then
  // put the temp file in place of the original and rebuild the index.
  bool rewriteRecipe(int index, const char* replacement);

  IndexEntry index_[MAX_RECIPES];
  int count_ = 0;
  int maxId_ = 0;  // highest id seen, so new recipes get a unique one
};
