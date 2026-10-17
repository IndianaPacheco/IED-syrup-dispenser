/*
  Settings.h
  ----------
  The WiFi network, its password, and the web PIN, saved on the SD
  card in SETTINGS_FILE so they never have to be in the code (or on
  GitHub). They are set on the machine in the Settings menu.

  The file is plain JSON, for example:
    {"wifi":"join","ssid":"Cafe","password":"secret123",
     "ap_password":"","pin":"1234"}

  Call loadSettings() after the SD card is started (RecipeManager's
  beginCard()).
*/

#pragma once

#include <Arduino.h>

enum class WifiMode : uint8_t {
  Off,   // no WiFi; the knob still works
  Join,  // join the network 'ssid'
  Own    // make our own network called AP_SSID
};

// Longest values WiFi allows, plus the '\0'.
constexpr int SSID_SIZE     = 33;
constexpr int PASSWORD_SIZE = 64;
constexpr int PIN_SIZE      = 9;   // up to 8 characters

struct NetworkSettings {
  WifiMode mode = WifiMode::Off;
  char ssid[SSID_SIZE]           = "";
  char password[PASSWORD_SIZE]   = "";  // for 'ssid'
  char apPassword[PASSWORD_SIZE] = "";  // for our own network; "" = open
  char pin[PIN_SIZE]             = "1234";
};

// Read the settings from the card. If the file is missing or broken,
// 'settings' keeps its defaults (WiFi off) and this returns false.
bool loadSettings(NetworkSettings& settings);

// Write the settings to the card. Returns false on any error, in
// which case the old file is left as it was.
bool saveSettings(const NetworkSettings& settings);
