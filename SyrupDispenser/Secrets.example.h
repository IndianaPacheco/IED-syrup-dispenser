/*
  Secrets.example.h
  -----------------
  Copy this file to Secrets.h (same folder) and fill it in.
  Secrets.h is in .gitignore so your WiFi password never ends up
  on GitHub.
*/

#pragma once

// The WiFi network the dispenser joins. Leave WIFI_SSID empty ("")
// to have the dispenser make its own network instead (see below),
// e.g. on school WiFi that needs a login page.
constexpr const char* WIFI_SSID     = "YourNetworkName";
constexpr const char* WIFI_PASSWORD = "YourNetworkPassword";

// Used only when WIFI_SSID is empty. Join this network from the
// register, then open http://192.168.4.1/
// The password must be at least 8 characters.
constexpr const char* AP_SSID     = "SyrupDispenser";
constexpr const char* AP_PASSWORD = "changeme123";

// The web page asks for this before it will send orders, so guests
// on the same WiFi can't pour drinks. Sent as plain HTTP: it keeps
// out casual button-pushers, not a determined attacker.
constexpr const char* REMOTE_PIN = "1234";
