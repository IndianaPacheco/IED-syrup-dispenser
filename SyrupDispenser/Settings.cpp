#include "Settings.h"
#include "Config.h"

#include <ArduinoJson.h>
#include <SD.h>

static const char* modeName(WifiMode mode) {
  switch (mode) {
    case WifiMode::Join: return "join";
    case WifiMode::Own:  return "own";
    default:             return "off";
  }
}

// Copy a string field from the JSON into 'out' if it is there.
static void readField(JsonDocument& doc, const char* key, char* out, size_t size) {
  const char* value = doc[key] | (const char*)nullptr;
  if (value != nullptr) {
    strncpy(out, value, size - 1);
    out[size - 1] = '\0';
  }
}

bool loadSettings(NetworkSettings& settings) {
  if (!SD.exists(SETTINGS_FILE)) {
    Serial.println("No settings file; WiFi stays off until set up.");
    return false;
  }

  File file = SD.open(SETTINGS_FILE, FILE_READ);
  if (!file) {
    Serial.println("Could not open the settings file.");
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) {
    Serial.print("Settings file is invalid: ");
    Serial.println(error.c_str());
    return false;
  }

  const char* mode = doc["wifi"] | "off";
  if (strcmp(mode, "join") == 0) {
    settings.mode = WifiMode::Join;
  } else if (strcmp(mode, "own") == 0) {
    settings.mode = WifiMode::Own;
  } else {
    settings.mode = WifiMode::Off;
  }

  readField(doc, "ssid",        settings.ssid,       sizeof(settings.ssid));
  readField(doc, "password",    settings.password,   sizeof(settings.password));
  readField(doc, "ap_password", settings.apPassword, sizeof(settings.apPassword));
  readField(doc, "pin",         settings.pin,        sizeof(settings.pin));
  return true;
}

bool saveSettings(const NetworkSettings& settings) {
  static constexpr const char* TEMP_FILE = "/settings.tmp";

  JsonDocument doc;
  doc["wifi"]        = modeName(settings.mode);
  doc["ssid"]        = settings.ssid;
  doc["password"]    = settings.password;
  doc["ap_password"] = settings.apPassword;
  doc["pin"]         = settings.pin;

  // Write a temp file first so a failed write can't wipe the
  // settings that are already there.
  SD.remove(TEMP_FILE);
  File temp = SD.open(TEMP_FILE, FILE_WRITE);
  if (!temp) {
    Serial.println("Cannot save settings: temp file did not open.");
    return false;
  }
  size_t expected = measureJson(doc);
  bool ok = serializeJson(doc, temp) == expected;
  temp.close();

  if (!ok) {
    SD.remove(TEMP_FILE);
    Serial.println("Cannot save settings: write failed (card full?).");
    return false;
  }

  SD.remove(SETTINGS_FILE);  // fine if it didn't exist yet
  if (!SD.rename(TEMP_FILE, SETTINGS_FILE)) {
    Serial.println("Cannot save settings: rename failed.");
    return false;
  }
  return true;
}
