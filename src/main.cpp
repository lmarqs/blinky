#include <Arduino.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>

#include "BlinkController.h"
#include "BlinkSetting.h"

// --- Hardware config (carrier boards vary; verify on hardware) ---

// A digital output bound to its active polarity. Pairing the pin with its
// active level keeps the two from being mismatched and puts the on/off→level
// mapping in one spot. Lives here, not lib/blink, because it touches Arduino
// GPIO — the pure lamp logic stays host-testable in BlinkController.
struct Output {
  uint8_t pin;
  uint8_t activeLevel;

  void begin() const {
    pinMode(pin, OUTPUT);
    write(false);  // off (HIGH for active-LOW GPIO2, which the boot strap wants)
  }

  void write(bool on) const { digitalWrite(pin, on ? activeLevel : (activeLevel == HIGH ? LOW : HIGH)); }
};

// GPIO0: relay + boot strap (LOW at reset = flash mode). GPIO2: onboard blue
// LED that mirrors the lamp, wired active-LOW. Flip an activeLevel if your
// carrier inverts it.
static const Output relay{0, HIGH};
static const Output led{2, LOW};

static const size_t EEPROM_SIZE = 64;

static BlinkController blink_;
static ESP8266WebServer server(80);

// Generated from assets/index.html by `mise run hexdump`.
#include "index.html.h"

// Drive the carrier outputs — the relay and the LED that mirrors it. Routing
// both through here keeps the LED in lockstep with the relay.
static void applyOutput() {
  const bool on = blink_.output();
  relay.write(on);
  led.write(on);
  Serial.printf("lamp %s\n", on ? "on" : "off");
}

// Advance the state machine and drive the outputs on changes.
static void tick() {
  if (blink_.update(millis())) {
    applyOutput();
  }
}

static void loadSetting() {
  EEPROM.begin(EEPROM_SIZE);
  BlinkSetting s{};
  EEPROM.get(0, s);
  applyBlinkSetting(s, blink_);
}

static void saveSetting() {
  const BlinkSetting s = blinkSettingFrom(blink_);
  EEPROM.put(0, s);
  EEPROM.commit();
}

static void sendStatus() {
  char body[96];

  snprintf(body, sizeof(body), "{\"mode\":\"%s\",\"period\":%lu,\"lamp\":%s}", blinkModeName(blink_.mode()),
           static_cast<unsigned long>(blink_.period()), blink_.output() ? "true" : "false");

  server.send(200, "application/json", body);
}

static void handleIndex() {
  server.send_P(200, "text/html", reinterpret_cast<const char*>(index_html), index_html_len);
}

static void handleMode() {
  BlinkMode mode;

  if (!parseBlinkMode(server.arg("mode").c_str(), mode)) {
    server.send(400, "application/json", "{\"error\":\"mode must be blink|on|off\"}");
    return;
  }

  if (mode != blink_.mode()) {
    blink_.setMode(mode);
    tick();
    saveSetting();
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
    saveSetting();
  }

  sendStatus();
}

void setup() {
  // GPIO0/GPIO2 are boot straps: claim them and set outputs off first thing.
  relay.begin();
  led.begin();

  Serial.begin(115200);
  loadSetting();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(BLINKY_AP_SSID, BLINKY_AP_PASSWORD);
  Serial.printf("\nAP \"%s\" up at %s\n", BLINKY_AP_SSID, WiFi.softAPIP().toString().c_str());

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
