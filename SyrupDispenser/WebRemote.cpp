#include "WebRemote.h"
#include "Config.h"
#include "Secrets.h"
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

WebRemote::WebRemote(const RecipeManager& recipes)
    : recipes_(recipes), server_(80) {}

bool WebRemote::begin(OrderHandler onOrder, StopHandler onStop,
                      StatusHandler onStatus) {
  onOrder_ = onOrder;
  onStop_ = onStop;
  onStatus_ = onStatus;

  bool connected;
  if (strlen(WIFI_SSID) == 0) {
    accessPoint_ = true;
    connected = WiFi.softAP(AP_SSID, AP_PASSWORD);
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(WEB_HOSTNAME);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
      delay(100);
    }
    connected = (WiFi.status() == WL_CONNECTED);
  }

  MDNS.begin(WEB_HOSTNAME);

  server_.on("/",       HTTP_GET,  [this] { handlePage(); });
  server_.on("/drinks", HTTP_GET,  [this] { handleDrinks(); });
  server_.on("/status", HTTP_GET,  [this] { handleStatus(); });
  server_.on("/order",  HTTP_POST, [this] { handleOrder(); });
  server_.on("/stop",   HTTP_POST, [this] { handleStop(); });
  server_.onNotFound([this] { server_.send(404, "text/plain", "Not found"); });
  server_.begin();

  return connected;
}

String WebRemote::address() const {
  if (accessPoint_) {
    return WiFi.softAPIP().toString();
  }
  if (WiFi.status() != WL_CONNECTED) {
    return "";
  }
  return WiFi.localIP().toString();
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
  }

  server_.handleClient();
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
  RemoteStatus status = onStatus_();

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

  switch (onOrder_(index, name.c_str())) {
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
  if (onStop_()) {
    server_.send(200, "text/plain", "Stopped.");
  } else {
    server_.send(200, "text/plain", "Nothing to stop.");
  }
}

bool WebRemote::checkPin() {
  if (server_.arg("pin") == REMOTE_PIN) {
    return true;
  }
  server_.send(401, "text/plain", "Wrong PIN.");
  return false;
}
