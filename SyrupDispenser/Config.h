/*
  Config.h
  --------
  Every hardware pin, calibration value, and system limit lives here.
  If you rewire the board or recalibrate a pump, this should be the
  only file you need to edit.
*/

#pragma once

#include <Arduino.h>

// ============================================================
// PUMPS
// ============================================================

constexpr int PUMP_COUNT = 8;

// One active-HIGH MOSFET driver input per pump (pump 1 .. pump 8).
constexpr int PUMP_PINS[PUMP_COUNT] = {
  13, 14, 16, 17, 25, 26, 27, 32
};

// Measured flow rate of each pump, in mL per second.
//
// These are PLACEHOLDERS. To calibrate a pump: run it for a known
// time (e.g. 10 s) with the real syrup and tubing, measure the
// volume, and divide. 50 mL in 10 s -> 5.0 mL/s.
constexpr float PUMP_ML_PER_SECOND[PUMP_COUNT] = {
  5.0f, 5.0f, 5.0f, 5.0f,
  5.0f, 5.0f, 5.0f, 5.0f
};

// Largest volume a recipe may request from a single pump.
constexpr float MAX_ML_PER_PUMP = 100.0f;

// Safety limit: refuse any dose that would run a pump longer than this.
constexpr uint32_t MAX_PUMP_RUN_MS = 600000;  // 10 minutes

// ============================================================
// microSD CARD (VSPI bus)
// ============================================================

constexpr int SD_CS_PIN   = 5;
constexpr int SD_SCK_PIN  = 18;
constexpr int SD_MISO_PIN = 19;
constexpr int SD_MOSI_PIN = 23;

constexpr uint32_t SD_SPI_HZ = 1000000;  // 1 MHz: slow but reliable

// One JSON recipe per line. See sd_card/recipes.jsonl for an example.
constexpr const char* RECIPE_FILE = "/recipes.jsonl";

// Recipe index limits. The index costs roughly
// MAX_RECIPES * (MAX_NAME_LENGTH + 8) bytes of RAM (~48 KB here).
constexpr int MAX_RECIPES     = 1000;
constexpr int MAX_NAME_LENGTH = 40;   // includes the '\0'
constexpr int MAX_JSON_LINE   = 512;  // longest allowed recipe line

// ============================================================
// LCD (16x2, I2C)
// ============================================================

constexpr uint8_t LCD_I2C_ADDRESS = 0x27;  // often 0x27 or 0x3F
constexpr int LCD_COLUMNS = 16;
constexpr int LCD_ROWS    = 2;
constexpr int LCD_SDA_PIN = 21;
constexpr int LCD_SCL_PIN = 22;

// ============================================================
// USER INPUTS
// ============================================================

// GPIO 34 and 35 are input-only and have NO internal pull-ups.
// Wire external 10 kOhm pull-up resistors to 3.3 V on DT and SW.
constexpr int ENCODER_CLK_PIN = 33;
constexpr int ENCODER_DT_PIN  = 34;
constexpr int ENCODER_SW_PIN  = 35;

// Stop button wired between this pin and GND.
// This is a SOFTWARE stop, not a safety-rated emergency stop.
constexpr int STOP_BUTTON_PIN = 4;

constexpr uint32_t DEBOUNCE_MS = 35;

// ============================================================
// DRINK PROGRAMMING (DrinkEditor)
// ============================================================

// mL added or removed per knob click when setting a pump amount.
constexpr float EDIT_ML_STEP = 0.5f;

// Clicks closer together than this count as a fast spin and take
// EDIT_ML_STEP * EDIT_FAST_MULTIPLIER per click instead.
constexpr uint32_t EDIT_FAST_TURN_MS    = 60;
constexpr float    EDIT_FAST_MULTIPLIER = 4.0f;
