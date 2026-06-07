#include <Arduino.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>

// The web server and WiFi headers differ by chip; alias both to HttpServer so
// the route handlers below stay platform-agnostic. The rest of the API
// (EEPROM, WiFi.softAP, ArduinoOTA, send_P) is identical across both cores.
#if defined(ESP8266)
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
using HttpServer = ESP8266WebServer;
#elif defined(ESP32)
#include <WebServer.h>
#include <WiFi.h>
using HttpServer = WebServer;
#endif

#include "BlinkController.h"
#include "BlinkSetting.h"
#include "BlinkSettingUpdate.h"

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
    write(false);  // off first thing; write() derives the off-level from activeLevel,
                   // so a boot-strap pin (e.g. active-LOW GPIO2) lands in its safe state
  }

  void write(bool on) const { digitalWrite(pin, on ? activeLevel : (activeLevel == HIGH ? LOW : HIGH)); }
};

// Pin map differs by board; the lamp logic (BlinkController) does not. Flip an
// activeLevel if your carrier/board inverts it.
#if defined(ESP8266)
// ESP-01S: GPIO0 = relay + boot strap (LOW at reset = flash mode). GPIO2 =
// onboard blue LED that mirrors the lamp, wired active-LOW.
static const Output relay{0, LOW};
static const Output led{2, LOW};
#elif defined(ESP32)
// ESP32 DevKit (WROOM-32): relay on GPIO23 — a plain output that stays LOW at
// boot, so it won't drive the relay during reset/flash. Avoid the strapping
// pins GPIO0/2/12/15. GPIO2 here is the onboard LED, active-HIGH on the DevKit
// (the opposite polarity to the ESP-01S).
static const Output relay{23, HIGH};
static const Output led{2, HIGH};
#endif

static const size_t EEPROM_SIZE = 64;

static BlinkController blink_;
static HttpServer server(80);

// Generated from assets/index.html by `mise run hexdump`.
#include "index.html.h"

// Drive the carrier outputs — the relay and the LED that mirrors it. Routing
// both through here keeps the LED in lockstep with the relay.
static void applyOutput() {
  const bool on = blink_.output().isOn();
  relay.write(on);
  led.write(on);
}

// Report the lamp transition over serial — separate from applyOutput so
// actuation and observability each have a single reason to change.
static void logLamp() { Serial.printf("lamp %s\n", blink_.output().name()); }

// Advance the state machine; on a real transition, drive the outputs and log it.
static void tick() {
  if (!blink_.update(millis())) {
    return;
  }
  applyOutput();
  logLamp();
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
  statusJson(blink_, body, sizeof(body));
  server.send(200, "application/json", body);
}

static void handleIndex() {
  server.send_P(200, "text/html", reinterpret_cast<const char*>(index_html), index_html_len);
}

// One response policy for every command route. Rejected: send the route's 400
// error. Accepted (the Setting changed): drive the outputs to the new lamp
// state, log it, and persist (write-on-change). Unchanged: do neither. Every
// non-rejected request ends by reporting the current status.
static void respondToCommand(BlinkOutcome outcome, const char* errorBody) {
  switch (outcome) {
    case BlinkOutcome::Rejected:
      server.send(400, "application/json", errorBody);
      return;
    case BlinkOutcome::Accepted:
      applyOutput();
      logLamp();
      saveSetting();
      break;
    case BlinkOutcome::Unchanged:
      break;
  }

  sendStatus();
}

static void handleMode() {
  respondToCommand(applyMode(blink_, server.arg("mode").c_str(), millis()),
                   "{\"error\":\"mode must be blink|on|off\"}");
}

static void handlePeriod() {
  respondToCommand(applyPeriod(blink_, server.arg("period").c_str(), millis()),
                   "{\"error\":\"period must be a positive integer\"}");
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
