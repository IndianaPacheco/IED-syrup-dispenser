/*
  SettingsMenu.h
  --------------
  The Settings entry at the end of the main menu. Knob only, no PIN:
  only staff can reach the machine.

    WiFi network   scan, then pick a network and type its password.
                   The list ends with Hidden network (type its name),
                   Own network (the dispenser makes AP_SSID), and
                   WiFi off.
    Web PIN        the PIN the barista page asks for
    Web address    what to open on the register, and the WiFi status
    Back

  Like DrinkEditor, this module only collects the changes: when it
  returns Result::Save the main sketch saves settings() to the SD
  card and reconnects.
*/

#pragma once

#include <Arduino.h>
#include "Display.h"
#include "Settings.h"
#include "TextEntry.h"
#include "WebRemote.h"

class SettingsMenu {
public:
  enum class Result {
    Open,  // still in the menu; nothing to do
    Save,  // settings() changed: save it and reconnect, stay open
    Exit   // back to the main menu
  };

  SettingsMenu(Display& display, const WebRemote& remote);

  // Open on the top list, editing a copy of 'current'.
  void start(const NetworkSettings& current);

  void turn(int step);
  Result press();
  void draw();

  // True while the Web address screen is out of date (e.g. WiFi just
  // connected). Call every loop and draw() when it says so.
  bool needsRedraw() const;

  const NetworkSettings& settings() const { return settings_; }

private:
  enum class Step { Top, Networks, HiddenName, Password, Pin, Address };

  // Scan for networks and open the network list. Takes a few seconds.
  void scan();

  // Ask for the password of 'pendingSsid_' (or of our own network).
  void askPassword();

  // List entries after the scanned networks.
  int hiddenChoice() const  { return scanCount_; }
  int ownChoice() const     { return scanCount_ + 1; }
  int offChoice() const     { return scanCount_ + 2; }
  int networkChoices() const { return scanCount_ + 3; }

  static constexpr int MAX_SCANNED = 12;

  Display&         display_;
  const WebRemote& remote_;
  NetworkSettings  settings_;

  Step step_ = Step::Top;
  int  choice_ = 0;
  TextEntry entry_;

  // Network chosen but not saved until its password is entered.
  WifiMode pendingMode_ = WifiMode::Off;
  char     pendingSsid_[SSID_SIZE] = "";

  char scanned_[MAX_SCANNED][SSID_SIZE];
  bool scannedOpen_[MAX_SCANNED];  // no password needed
  int  scanCount_ = 0;

  String shownAddress_;  // address on the Address screen when drawn
};
