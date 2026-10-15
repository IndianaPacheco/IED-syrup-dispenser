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

  // Scan the recipe file once and build the in-memory index.
  // Invalid lines are skipped (and reported on Serial).
  // Returns false if no valid recipes were found.
  bool buildIndex();

  // Number of valid recipes found by buildIndex().
  int count() const { return count_; }

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

  IndexEntry index_[MAX_RECIPES];
  int count_ = 0;
};
