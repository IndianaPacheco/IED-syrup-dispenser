#include "SettingsMenu.h"
#include "Config.h"

#include <WiFi.h>

// Top list, in knob order.
enum TopItem { TOP_NETWORK, TOP_PIN, TOP_ADDRESS, TOP_BACK, TOP_COUNT };
static const char* const TOP_LABELS[TOP_COUNT] = {
  "WiFi network", "Web PIN", "Web address", "Back"
};

SettingsMenu::SettingsMenu(Display& display, const WebRemote& remote)
    : display_(display), remote_(remote) {}

void SettingsMenu::start(const NetworkSettings& current) {
  settings_ = current;
  step_ = Step::Top;
  choice_ = TOP_NETWORK;
}

void SettingsMenu::turn(int step) {
  switch (step_) {
    case Step::Top:
      choice_ = (choice_ + step + TOP_COUNT) % TOP_COUNT;
      break;
    case Step::Networks:
      choice_ = (choice_ + step + networkChoices()) % networkChoices();
      break;
    case Step::HiddenName:
    case Step::Password:
    case Step::Pin:
      entry_.turn(step);
      break;
    case Step::Address:
      break;
  }
}

SettingsMenu::Result SettingsMenu::press() {
  switch (step_) {
    case Step::Top:
      switch (choice_) {
        case TOP_NETWORK:
          scan();
          break;
        case TOP_PIN:
          step_ = Step::Pin;
          entry_.start("PIN:", settings_.pin, PIN_SIZE - 1, 1, false,
                       TextEntry::DIGITS);
          break;
        case TOP_ADDRESS:
          step_ = Step::Address;
          break;
        case TOP_BACK:
          return Result::Exit;
      }
      break;

    case Step::Networks:
      if (choice_ < scanCount_) {
        pendingMode_ = WifiMode::Join;
        strcpy(pendingSsid_, scanned_[choice_]);
        if (scannedOpen_[choice_]) {
          settings_.mode = WifiMode::Join;
          strcpy(settings_.ssid, pendingSsid_);
          settings_.password[0] = '\0';
          step_ = Step::Address;  // watch it connect
          return Result::Save;
        }
        askPassword();
      } else if (choice_ == hiddenChoice()) {
        pendingMode_ = WifiMode::Join;
        step_ = Step::HiddenName;
        entry_.start("WiFi:", "", SSID_SIZE - 1, 1, false);
      } else if (choice_ == ownChoice()) {
        pendingMode_ = WifiMode::Own;
        askPassword();
      } else if (choice_ == offChoice()) {
        settings_.mode = WifiMode::Off;
        step_ = Step::Top;
        choice_ = TOP_NETWORK;
        return Result::Save;
      }
      break;

    case Step::HiddenName:
      switch (entry_.press()) {
        case TextEntry::Result::Done:
          strcpy(pendingSsid_, entry_.text());
          askPassword();
          break;
        case TextEntry::Result::Cancel:
          step_ = Step::Networks;
          break;
        case TextEntry::Result::Editing:
          break;
      }
      break;

    case Step::Password:
      switch (entry_.press()) {
        case TextEntry::Result::Done:
          settings_.mode = pendingMode_;
          if (pendingMode_ == WifiMode::Own) {
            strcpy(settings_.apPassword, entry_.text());
          } else {
            strcpy(settings_.ssid, pendingSsid_);
            strcpy(settings_.password, entry_.text());
          }
          step_ = Step::Address;  // watch it connect
          return Result::Save;
        case TextEntry::Result::Cancel:
          step_ = Step::Networks;
          break;
        case TextEntry::Result::Editing:
          break;
      }
      break;

    case Step::Pin:
      switch (entry_.press()) {
        case TextEntry::Result::Done:
          strcpy(settings_.pin, entry_.text());
          step_ = Step::Top;
          return Result::Save;
        case TextEntry::Result::Cancel:
          step_ = Step::Top;
          break;
        case TextEntry::Result::Editing:
          break;
      }
      break;

    case Step::Address:
      step_ = Step::Top;
      choice_ = TOP_ADDRESS;
      break;
  }
  return Result::Open;
}

void SettingsMenu::draw() {
  switch (step_) {
    case Step::Top:
      display_.showList("Settings", TOP_LABELS, TOP_COUNT, choice_);
      break;

    case Step::Networks: {
      const char* items[MAX_SCANNED + 3];
      for (int i = 0; i < scanCount_; i++) {
        items[i] = scanned_[i];
      }
      items[hiddenChoice()] = "Hidden network";
      items[ownChoice()]    = "Own network";
      items[offChoice()]    = "WiFi off";
      display_.showList("Pick WiFi network", items, networkChoices(), choice_);
      break;
    }

    case Step::HiddenName:
    case Step::Password:
    case Step::Pin:
      entry_.draw(display_);
      break;

    case Step::Address: {
      shownAddress_ = remote_.address();

      char network[40];
      const char* status;
      switch (settings_.mode) {
        case WifiMode::Join:
          snprintf(network, sizeof(network), "On: %s", settings_.ssid);
          status = "Connecting...";
          break;
        case WifiMode::Own:
          snprintf(network, sizeof(network), "Own: %s", AP_SSID);
          status = "Starting...";
          break;
        default:
          strcpy(network, "WiFi is off");
          status = "";
          break;
      }

      char pin[LCD_COLUMNS + 1];
      snprintf(pin, sizeof(pin), "PIN: %s", settings_.pin);
      display_.showNetworkInfo(network,
                               shownAddress_.length() ? shownAddress_.c_str() : status,
                               pin);
      break;
    }
  }
}

bool SettingsMenu::needsRedraw() const {
  return step_ == Step::Address && remote_.address() != shownAddress_;
}

void SettingsMenu::scan() {
  display_.showMessage("Scanning WiFi...");

  // Scanning needs the WiFi radio's station side on. Turn it on
  // without dropping our own network if we have one.
  wifi_mode_t mode = WiFi.getMode();
  if (mode == WIFI_OFF) {
    WiFi.mode(WIFI_STA);
  } else if (mode == WIFI_AP) {
    WiFi.mode(WIFI_AP_STA);
  }

  int found = WiFi.scanNetworks();  // blocks for a few seconds
  scanCount_ = 0;

  // Strongest first is how the scan returns them. Skip hidden
  // networks (no name) and repeats from multiple access points.
  for (int i = 0; i < found && scanCount_ < MAX_SCANNED; i++) {
    String name = WiFi.SSID(i);
    if (name.length() == 0 || name.length() >= SSID_SIZE) {
      continue;
    }
    bool repeat = false;
    for (int j = 0; j < scanCount_; j++) {
      repeat = repeat || name == scanned_[j];
    }
    if (repeat) {
      continue;
    }
    strcpy(scanned_[scanCount_], name.c_str());
    scannedOpen_[scanCount_] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
    scanCount_++;
  }
  WiFi.scanDelete();

  if (mode == WIFI_OFF) {
    WiFi.mode(WIFI_OFF);
  } else if (mode == WIFI_AP) {
    WiFi.mode(WIFI_AP);
  }

  step_ = Step::Networks;
  choice_ = 0;
}

void SettingsMenu::askPassword() {
  step_ = Step::Password;

  // WPA passwords are 8 to 63 characters; empty means an open network.
  if (pendingMode_ == WifiMode::Own) {
    entry_.start("AP pass:", settings_.apPassword, PASSWORD_SIZE - 1, 8, true);
  } else {
    // Retyping the same network's password starts from the saved one.
    bool same = strcmp(pendingSsid_, settings_.ssid) == 0;
    entry_.start("Pass:", same ? settings_.password : "", PASSWORD_SIZE - 1,
                 8, true);
  }
}
