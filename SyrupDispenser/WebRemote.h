/*
  WebRemote.h
  -----------
  Lets a barista order drinks from the register (or any phone or
  tablet) on the same WiFi network.

  The ESP32 joins the WiFi in Secrets.h, or makes its own network if
  WIFI_SSID is empty, and serves a web page at http://<address>/.
  The page lists the drinks, shows what the machine is doing, and
  has a Stop button.

  A web order never pours by itself: the machine shows it and waits
  for someone to put a cup down and press the knob.

  This module only handles the network. The main sketch decides
  whether to accept an order (see begin()).
*/

#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include "RecipeManager.h"

enum class OrderResult {
  Accepted,  // shown on the machine, waiting for the knob press
  Busy,      // someone is using the machine, or it is pouring
  Changed    // the drink list changed since the page loaded it
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

  explicit WebRemote(const RecipeManager& recipes);

  // Connect to WiFi (waiting up to WIFI_CONNECT_TIMEOUT_MS) and start
  // the web server. Returns false if WiFi didn't connect. It keeps
  // retrying in the background, and the machine works without it.
  bool begin(OrderHandler onOrder, StopHandler onStop, StatusHandler onStatus);

  // What to type in the browser, e.g. "192.168.1.50".
  String address() const;

  // Answer waiting web requests. Call once every loop().
  void poll();

private:
  void handlePage();
  void handleDrinks();
  void handleStatus();
  void handleOrder();
  void handleStop();

  // True if the request carries the right PIN; otherwise answers
  // it with an error.
  bool checkPin();

  const RecipeManager& recipes_;
  WebServer     server_;
  OrderHandler  onOrder_  = nullptr;
  StopHandler   onStop_   = nullptr;
  StatusHandler onStatus_ = nullptr;
  bool accessPoint_ = false;  // made our own network
  bool announced_   = false;  // printed the address on Serial yet
};
