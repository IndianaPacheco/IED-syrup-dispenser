/*
  WebRemote.h
  -----------
  Lets a barista order drinks from the register (or any phone or
  tablet) on the same WiFi network.

  The ESP32 joins WiFi or makes its own network, as set in the
  Settings menu (see Settings.h), and serves a web page at
  http://<address>/. The page lists the drinks, shows what the
  machine is doing, and has a Stop button. Its Drinks tab adds,
  edits, and deletes drinks.

  A web order never pours by itself: the machine shows it and waits
  for someone to put a cup down and press the knob.

  This module only handles the network and checks what the page
  sends. The main sketch decides whether to accept an order or a
  drink change (see RemoteHandlers).
*/

#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include "RecipeManager.h"
#include "Settings.h"

enum class OrderResult {
  Accepted,  // shown on the machine, waiting for the knob press
  Busy,      // someone is using the machine, or it is pouring
  Changed    // the drink list changed since the page loaded it
};

enum class EditResult {
  Done,     // saved or deleted
  Busy,     // someone is using the machine, or it is pouring
  Changed,  // the drink list changed since the page loaded it
  Full,     // no room for another drink
  Failed    // SD card error
};

// What the machine is doing, for the page's status line.
struct RemoteStatus {
  const char* state;  // "idle", "waiting", "pouring", "done", ...
  const char* drink;  // drink involved, or "" if none
};

class WebRemote {
public:
  // The page ordered recipe 'index'. 'name' is the name the page
  // showed, so a list that went stale (drink edited or deleted on
  // the machine) is caught instead of pouring the wrong drink.
  using OrderHandler = OrderResult (*)(int index, const char* name);

  // The page pressed Stop. Returns true if something was stopped.
  using StopHandler = bool (*)();

  using StatusHandler = RemoteStatus (*)();

  // Save a drink from the page. index is -1 for a new drink; for an
  // existing one, oldName is the name the page showed (as above).
  // name and amountsMl are already checked.
  using SaveHandler = EditResult (*)(int index, const char* oldName,
                                     const char* name,
                                     const float amountsMl[PUMP_COUNT]);

  // Delete drink 'index', which the page showed as 'oldName'.
  using DeleteHandler = EditResult (*)(int index, const char* oldName);

  struct RemoteHandlers {
    OrderHandler  order;
    StopHandler   stop;
    StatusHandler status;
    SaveHandler   save;
    DeleteHandler remove;
  };

  // 'settings' is read again by every reconnect() and PIN check, so
  // it must stay alive (the main sketch's copy).
  WebRemote(const RecipeManager& recipes, const NetworkSettings& settings);

  // Connect to WiFi (waiting up to WIFI_CONNECT_TIMEOUT_MS) and start
  // the web server. Returns false if WiFi didn't connect. It keeps
  // retrying in the background, and the machine works without it.
  bool begin(const RemoteHandlers& handlers);

  // Drop the current WiFi and start again with the settings as they
  // are now (after the Settings menu changed them). Doesn't wait.
  void reconnect();

  // What to type in the browser, e.g. "192.168.1.50", or "" while
  // not connected.
  String address() const;

  // Answer waiting web requests. Call once every loop().
  void poll();

private:
  // Start WiFi from settings_. With 'wait', block until connected or
  // WIFI_CONNECT_TIMEOUT_MS. Returns true if connected.
  bool connect(bool wait);

  void handlePage();
  void handleDrinks();
  void handleStatus();
  void handleOrder();
  void handleStop();
  void handleRecipe();
  void handleSave();
  void handleDelete();

  // Answer a save or delete with a message for the page.
  void sendEditResult(EditResult result, const char* doneMessage);

  // True if the request carries the right PIN; otherwise answers
  // it with an error.
  bool checkPin();

  const RecipeManager&   recipes_;
  const NetworkSettings& settings_;
  WebServer     server_;
  RemoteHandlers handlers_ = {};
  bool announced_ = false;  // printed the address on Serial yet
  bool serverStarted_ = false;
};
