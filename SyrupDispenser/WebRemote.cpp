#include "WebRemote.h"
#include "Config.h"
#include "WebPage.h"

#include <ESPmDNS.h>
#include <WiFi.h>

// Add 'text' to 'out' as a quoted JSON string.
static void appendJsonString(String& out, const char* text) {
  out += '"';
  for (const char* c = text; *c; c++) {
    if (*c == '"' || *c == '\\') {
      out += '\\';
    }
    if ((uint8_t)*c >= 0x20) {  // drop control characters
      out += *c;
    }
  }
  out += '"';
}

WebRemote::WebRemote(const RecipeManager& recipes,
                     const NetworkSettings& settings)
    : recipes_(recipes), settings_(settings), server_(80) {}

bool WebRemote::begin(const RemoteHandlers& handlers) {
  handlers_ = handlers;

  bool connected = connect(true);

  server_.on("/",       HTTP_GET,  [this] { handlePage(); });
  server_.on("/drinks", HTTP_GET,  [this] { handleDrinks(); });
  server_.on("/status", HTTP_GET,  [this] { handleStatus(); });
  server_.on("/order",  HTTP_POST, [this] { handleOrder(); });
  server_.on("/stop",   HTTP_POST, [this] { handleStop(); });
  server_.on("/recipe", HTTP_GET,  [this] { handleRecipe(); });
  server_.on("/save",   HTTP_POST, [this] { handleSave(); });
  server_.on("/delete", HTTP_POST, [this] { handleDelete(); });
  server_.onNotFound([this] { server_.send(404, "text/plain", "Not found"); });
  return connected;
}

void WebRemote::reconnect() {
  connect(false);
}

bool WebRemote::connect(bool wait) {
  // Start clean: drop any network we were on or were hosting.
  MDNS.end();
  WiFi.disconnect(true);
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  announced_ = false;

  switch (settings_.mode) {
    case WifiMode::Off:
      Serial.println("WiFi is off (turn it on in Settings).");
      return false;

    case WifiMode::Own: {
      // An empty password makes an open network; the PIN still
      // guards the page.
      const char* password = settings_.apPassword[0] ? settings_.apPassword : nullptr;
      WiFi.mode(WIFI_AP);
      if (!WiFi.softAP(AP_SSID, password)) {
        Serial.println("Could not start our own WiFi network.");
        return false;
      }
      MDNS.begin(WEB_HOSTNAME);
      return true;
    }

    case WifiMode::Join:
      break;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(WEB_HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.begin(settings_.ssid, settings_.password);
  MDNS.begin(WEB_HOSTNAME);

  if (wait) {
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
      delay(100);
    }
  }
  return WiFi.status() == WL_CONNECTED;
}

String WebRemote::address() const {
  switch (settings_.mode) {
    case WifiMode::Own:
      return WiFi.softAPIP().toString();
    case WifiMode::Join:
      if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString();
      }
      return "";
    default:
      return "";
  }
}

void WebRemote::poll() {
  // WiFi may connect after setup() gave up waiting, so say where
  // the page is once it does.
  String where = address();
  if (!announced_ && where.length() > 0) {
    announced_ = true;
    Serial.print("Barista page: http://");
    Serial.print(where);
    Serial.print("/  or  http://");
    Serial.print(WEB_HOSTNAME);
    Serial.println(".local/");

    // The server can only start once there is a network.
    if (!serverStarted_) {
      server_.begin();
      serverStarted_ = true;
    }
  }

  if (serverStarted_) {
    server_.handleClient();
  }
}

void WebRemote::handlePage() {
  server_.send_P(200, "text/html", WEB_PAGE);
}

// The drink names as a JSON array. A drink's position in the array
// is its recipe index.
void WebRemote::handleDrinks() {
  // There can be up to MAX_RECIPES drinks, so send the list in
  // pieces instead of building it all in RAM.
  server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server_.send(200, "application/json", "");

  String chunk = "[";
  for (int i = 0; i < recipes_.count(); i++) {
    if (i > 0) {
      chunk += ',';
    }
    appendJsonString(chunk, recipes_.name(i));

    if (chunk.length() > 1024) {
      server_.sendContent(chunk);
      chunk = "";
    }
  }
  chunk += ']';
  server_.sendContent(chunk);
  server_.sendContent("");  // end of the list
}

void WebRemote::handleStatus() {
  RemoteStatus status = handlers_.status();

  String json = "{\"state\":";
  appendJsonString(json, status.state);
  json += ",\"drink\":";
  appendJsonString(json, status.drink);
  json += '}';

  server_.send(200, "application/json", json);
}

void WebRemote::handleOrder() {
  if (!checkPin()) {
    return;
  }
  if (!server_.hasArg("index") || !server_.hasArg("name")) {
    server_.send(400, "text/plain", "Bad order");
    return;
  }

  int index = server_.arg("index").toInt();
  String name = server_.arg("name");

  switch (handlers_.order(index, name.c_str())) {
    case OrderResult::Accepted:
      server_.send(200, "text/plain", "Sent. Place a cup and press the knob.");
      break;
    case OrderResult::Busy:
      server_.send(409, "text/plain", "Machine is busy. Try again in a moment.");
      break;
    case OrderResult::Changed:
      server_.send(409, "text/plain", "Drink list changed. Reloaded, try again.");
      break;
  }
}

void WebRemote::handleStop() {
  if (!checkPin()) {
    return;
  }
  if (handlers_.stop()) {
    server_.send(200, "text/plain", "Stopped.");
  } else {
    server_.send(200, "text/plain", "Nothing to stop.");
  }
}

// One drink's name and amounts, for the page's drink editor:
// {"name": "Vanilla Latte", "amounts": [10, 0, 0, 0, 0, 0, 0, 0]}
void WebRemote::handleRecipe() {
  int index = server_.arg("index").toInt();
  float amountsMl[PUMP_COUNT];
  if (!server_.hasArg("index") || !recipes_.loadAmounts(index, amountsMl)) {
    server_.send(404, "text/plain", "Drink not found. Reload the list.");
    return;
  }

  String json = "{\"name\":";
  appendJsonString(json, recipes_.name(index));
  json += ",\"amounts\":[";
  for (int i = 0; i < PUMP_COUNT; i++) {
    if (i > 0) {
      json += ',';
    }
    json += String(amountsMl[i], 1);
  }
  json += "]}";
  server_.send(200, "application/json", json);
}

// Add or replace a drink. Expects index (-1 = new), old (the name
// the page showed), name, and a0 .. a7 (mL for pumps 1 .. 8).
void WebRemote::handleSave() {
  if (!checkPin()) {
    return;
  }

  // Same rules as the drink editor on the machine, plus printable
  // ASCII only, since the LCD can't show anything else.
  String name = server_.arg("name");
  name.trim();
  if (name.length() == 0 || name.length() > MAX_NAME_LENGTH - 1) {
    String message = "Name must be 1 to " + String(MAX_NAME_LENGTH - 1) +
                     " characters.";
    server_.send(400, "text/plain", message);
    return;
  }
  for (unsigned int i = 0; i < name.length(); i++) {
    if (name[i] < 0x20 || name[i] > 0x7E) {
      server_.send(400, "text/plain",
                   "Name can only use plain letters, numbers, and symbols.");
      return;
    }
  }

  float amountsMl[PUMP_COUNT];
  bool anyAmount = false;
  for (int i = 0; i < PUMP_COUNT; i++) {
    String value = server_.arg("a" + String(i));
    value.trim();
    float amount = value.length() ? value.toFloat() : 0;
    if (!isfinite(amount) || amount < 0 || amount > MAX_ML_PER_PUMP) {
      String message = "Pump amounts must be 0 to " +
                       String(MAX_ML_PER_PUMP, 0) + " mL.";
      server_.send(400, "text/plain", message);
      return;
    }
    amountsMl[i] = amount;
    anyAmount = anyAmount || amount > 0;
  }
  if (!anyAmount) {
    server_.send(400, "text/plain", "At least one pump needs an amount.");
    return;
  }

  int index = server_.hasArg("index") ? server_.arg("index").toInt() : -1;
  String oldName = server_.arg("old");
  sendEditResult(handlers_.save(index, oldName.c_str(), name.c_str(), amountsMl),
                 "Saved.");
}

// Expects index and old (the name the page showed).
void WebRemote::handleDelete() {
  if (!checkPin()) {
    return;
  }
  if (!server_.hasArg("index") || !server_.hasArg("old")) {
    server_.send(400, "text/plain", "Bad request");
    return;
  }

  int index = server_.arg("index").toInt();
  String oldName = server_.arg("old");
  sendEditResult(handlers_.remove(index, oldName.c_str()), "Deleted.");
}

void WebRemote::sendEditResult(EditResult result, const char* doneMessage) {
  switch (result) {
    case EditResult::Done:
      server_.send(200, "text/plain", doneMessage);
      break;
    case EditResult::Busy:
      server_.send(409, "text/plain",
                   "Machine is in use. Try again when it is on the menu.");
      break;
    case EditResult::Changed:
      server_.send(409, "text/plain", "Drink list changed. Reloaded, try again.");
      break;
    case EditResult::Full:
      server_.send(409, "text/plain", "No room for more drinks.");
      break;
    case EditResult::Failed:
      server_.send(500, "text/plain", "SD card error. Nothing was changed.");
      break;
  }
}

bool WebRemote::checkPin() {
  if (server_.arg("pin") == settings_.pin) {
    return true;
  }
  server_.send(401, "text/plain", "Wrong PIN.");
  return false;
}
