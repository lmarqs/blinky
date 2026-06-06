#include <Arduino.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>

#include "BlinkController.h"

// --- Hardware config (carrier boards vary; verify on hardware) ---
static const uint8_t RELAY_PIN = 0;  // GPIO0 — also a boot strap, see CLAUDE.md
static const uint8_t RELAY_ACTIVE_LEVEL = HIGH;

// --- Settings persisted to EEPROM (flash-emulated) ---
struct Settings {
  uint8_t magic;
  uint8_t version;
  uint8_t mode;
  uint32_t periodMs;
};

static const uint8_t SETTINGS_MAGIC = 0xB1;
static const uint8_t SETTINGS_VERSION = 1;
static const size_t EEPROM_SIZE = 64;

static BlinkController blink_;
static ESP8266WebServer server(80);

// Generated from assets/index.html by `mise run hexdump`.
#include "index.html.h"

static void applyRelay() {
  digitalWrite(RELAY_PIN, blink_.output() ? RELAY_ACTIVE_LEVEL
                                          : (RELAY_ACTIVE_LEVEL == HIGH ? LOW : HIGH));
}

// Advance the state machine and drive the relay on changes.
static void tick() {
  if (blink_.update(millis())) {
    applyRelay();
  }
}

static void loadSettings() {
  EEPROM.begin(EEPROM_SIZE);
  Settings s{};
  EEPROM.get(0, s);
  const bool valid = s.magic == SETTINGS_MAGIC && s.version == SETTINGS_VERSION &&
                     s.mode <= static_cast<uint8_t>(BlinkMode::BLINK);
  if (valid) {
    blink_.restore(static_cast<BlinkMode>(s.mode), s.periodMs);
  }
}

static void saveSettings() {
  Settings s{SETTINGS_MAGIC, SETTINGS_VERSION, static_cast<uint8_t>(blink_.mode()),
             blink_.period()};
  EEPROM.put(0, s);
  EEPROM.commit();
}

static const char* modeName(BlinkMode mode) {
  switch (mode) {
    case BlinkMode::ON:
      return "on";
    case BlinkMode::OFF:
      return "off";
    default:
      return "blink";
  }
}

static bool parseMode(const String& name, BlinkMode& out) {
  if (name == "on") {
    out = BlinkMode::ON;
  } else if (name == "off") {
    out = BlinkMode::OFF;
  } else if (name == "blink") {
    out = BlinkMode::BLINK;
  } else {
    return false;
  }
  return true;
}

static void sendStatus() {
  char body[96];
  snprintf(body, sizeof(body), "{\"mode\":\"%s\",\"period\":%lu,\"output\":%s}",
           modeName(blink_.mode()), static_cast<unsigned long>(blink_.period()),
           blink_.output() ? "true" : "false");
  server.send(200, "application/json", body);
}

static void handleIndex() {
  server.send_P(200, "text/html", reinterpret_cast<const char*>(index_html),
                index_html_len);
}

static void handleMode() {
  BlinkMode mode;
  if (!parseMode(server.arg("mode"), mode)) {
    server.send(400, "application/json", "{\"error\":\"mode must be blink|on|off\"}");
    return;
  }
  if (mode != blink_.mode()) {
    blink_.setMode(mode);
    tick();
    saveSettings();
  }
  sendStatus();
}

static void handlePeriod() {
  const long requested = server.arg("period").toInt();
  if (requested <= 0) {
    server.send(400, "application/json", "{\"error\":\"period must be a positive integer\"}");
    return;
  }
  const uint32_t before = blink_.period();
  blink_.setPeriod(static_cast<uint32_t>(requested));
  if (blink_.period() != before) {
    saveSettings();
  }
  sendStatus();
}

void setup() {
  // GPIO0 is a boot strap: claim it and de-energize the relay first thing.
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LEVEL == HIGH ? LOW : HIGH);

  Serial.begin(115200);
  loadSettings();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(BLINKY_AP_SSID, BLINKY_AP_PASSWORD);
  Serial.printf("\nAP \"%s\" up at %s\n", BLINKY_AP_SSID,
                WiFi.softAPIP().toString().c_str());

  ArduinoOTA.setPassword(BLINKY_OTA_PASSWORD);
  ArduinoOTA.begin();

  server.on("/", HTTP_GET, handleIndex);
  server.on("/status", HTTP_GET, sendStatus);
  server.on("/mode", HTTP_POST, handleMode);
  server.on("/period", HTTP_POST, handlePeriod);
  server.onNotFound([]() { server.send(404, "text/plain", "not found"); });
  server.begin();
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  tick();
}
