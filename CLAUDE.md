# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Firmware for an ESP-01S (ESP8266) socketed in a relay carrier board that blinks a mains lamp. The device is a self-contained WiFi hotspot (SoftAP only — it never joins a network); a phone joins the `blinky` AP and controls it at `http://192.168.4.1`. Default behavior: blink every 5s. Mode (`blink|on|off`) and period are settable over HTTP and persist to EEPROM.

The same firmware also targets an ESP32 DevKit (WROOM-32) for bench development — the primary board is still the ESP-01S (`default_envs = esp01_1m`). ESP32 support is build-config + a small conditional block in `src/main.cpp` only; the pure `lib/blink/` core is shared unchanged.

## Commands

Everything goes through mise (it loads `.env` and provides the python venv with PlatformIO — bare `pio` invocations outside mise will miss the env vars that `platformio.ini` injects via `${sysenv.*}`):

- `mise run setup` — create `.env` from `.env.example`, install PlatformIO + clang-format into `.venv` from `requirements.txt`
- `mise run test` — host-native unit tests (`pio test -e native`); single suite: `pio test -e native -f test_blink`
- `mise run format` — clang-format `-i` over `src/`, `lib/`, `test/` (`.cpp`/`.h`); `mise run format-check` is the read-only `--Werror` variant CI gates on. Style is `.clang-format` (Google base, 120 cols). clang-format is pinned in `requirements.txt` so local and CI agree; `assets/*.h` is generated, so the tasks skip it.
- `mise run build:esp01` — compile firmware (`esp01_1m` env)
- `mise run upload:esp01` — wired flash via CH340 (chip must be **out** of the relay carrier and socketed in the CH340 adapter)
- `mise run ota:esp01` — flash over WiFi via espota to `192.168.4.1`; **the laptop must be joined to the blinky AP**
- `mise run build:esp32` / `upload:esp32` / `ota:esp32` — the ESP32 DevKit (`esp32dev`) variants of the three above. Tasks are namespaced `<action>:<board>`. Wired flash goes over the DevKit's onboard USB-serial — no carrier/chip-removal dance.
- `mise run monitor` — serial monitor at 115200
- `mise run hexdump` — regenerate C headers from `assets/` web assets (runs automatically before build/upload/ota)
- `mise run run` — the full first-flash loop: `build`, then wired `upload`, then `monitor`

## Architecture

- `lib/blink/BlinkController.{h,cpp}` — pure state machine, **no Arduino includes**. The clock is passed into `update(nowMs)`, which makes it host-testable and millis()-rollover-safe (unsigned subtraction idiom). All behavior logic (modes, period clamping 200ms–1h, toggle timing) lives here.
- `lib/blink/BlinkSetting.{h,cpp}` — pure too: mode↔name mapping (`blinkModeName`/`parseBlinkMode`) and the persisted record (`BlinkSetting` struct, magic/version validation via `applyBlinkSetting`/`blinkSettingFrom`). The EEPROM I/O itself stays in `src/main.cpp`.
- `lib/blink/BlinkOutput.{h,cpp}` — pure value object for the lamp's on/off state: `isOn()` plus the on/off `name()`. `BlinkController::output()` returns it, keeping the output vocabulary in one host-tested place instead of a bare `bool`.
- `src/main.cpp` — device glue only: SoftAP, sync web-server routes (`GET /`, `GET /status`, `POST /mode`, `POST /period`), ArduinoOTA, EEPROM persistence (magic/version struct, write-on-change), relay pin driving. The web-server/WiFi headers and the pin map are the only platform-specific parts: a top-of-file `#if defined(ESP8266) / #elif defined(ESP32)` block picks the headers and aliases the server class to `HttpServer`; everything below it is platform-neutral.
- `assets/index.html` — the web UI source. The `hexdump` mise task converts every non-`.h` file in `assets/` into a `const ... PROGMEM` byte-array header next to it (e.g. `assets/index.html.h`, symbols `index_html` / `index_html_len`). Generated headers are gitignored; edit the `.html`, never the `.h`. `platformio.ini` adds `-I assets` so `src/main.cpp` includes it directly.
- `test/test_*/test_main.cpp` — Unity tests, one suite per `lib/blink` module (`test_blink`, `test_blink_setting`, `test_blink_output`). Run only in the `native` env (`test_ignore = *` on embedded envs, `test_build_src = no` on native keeps Arduino-dependent `src/` out of host builds).

Upload envs in `platformio.ini`: `esp01_1m`/`esp01_1m_ota` (ESP8266) and `esp32dev`/`esp32dev_ota` (ESP32) — each board has a wired (esptool) and an OTA (espota with `--auth`) variant. A shared `[common]` section holds `framework`, `monitor_speed`, the secret defines, and `test_ignore`; per-chip `[esp_common]`/`[esp32_common]` add `platform`+`board`. Secrets (`BLINKY_AP_SSID`, `BLINKY_AP_PASSWORD`, `BLINKY_OTA_PASSWORD`) come from gitignored `.env` as compile-time string defines — the whole build flag is single-quoted (`'-D X="${sysenv.X}"'`), which is required for string defines to survive. The default `esp32dev` partition scheme is OTA-capable, so OTA works without a custom partition table.

## Hardware gotchas

- **GPIO0 is both the relay pin and a boot strap** (LOW at reset = flash mode). `setup()` claims the pin and de-energizes the relay first thing (`relay.begin()`). Each output is an `Output{pin, activeLevel}` struct in `src/main.cpp`; the relay's `activeLevel` is a compile-time value. **IMPORTANT: carrier boards vary — a wrong `activeLevel` energizes the mains lamp inverted (and at boot). Verify on hardware before flashing; flip it if the relay logic is inverted.**
- **The onboard blue LED (GPIO2) mirrors the lamp.** It's wired active-LOW on the ESP-01S, so it's declared `led{2, LOW}` in `src/main.cpp` (flip its `activeLevel` like the relay's if your board differs). GPIO2 is also a boot strap (HIGH at reset); "off" leaves it HIGH, so it stays satisfied. Both outputs are driven from one `applyOutput()` off `blink_.output()`, so the LED can't drift out of sync with the relay.
- **IMPORTANT: wired flashing requires the chip OUT of the relay carrier** (socketed in the CH340 adapter) — flashing in-carrier drives GPIO0/the relay during boot. That's why OTA exists; prefer `mise run ota:esp01` after the first flash.
- **ESP32 DevKit (`esp32dev`):** relay is on **GPIO23** (a plain output that stays LOW at boot) — deliberately *not* GPIO0, to avoid driving the relay during reset/flash. **Avoid the ESP32 strapping pins GPIO0/2/12/15** when adding outputs. The onboard LED on GPIO2 is **active-HIGH** here (`led{2, HIGH}`) — the opposite polarity to the ESP-01S, so don't copy `led{2, LOW}` across. The DevKit flashes over its own USB-serial, so none of the carrier-removal caveat above applies. The same `activeLevel`-verify-on-hardware warning holds.
- The ESP8266 boot ROM prints its reset banner at **74880 baud** (`pio device monitor -b 74880` to read boot diagnostics); the app `Serial` runs at 115200. (The ESP32 boot ROM logs at 115200 already — same as the app.)
- ESP8266 **and ESP32** EEPROM is flash-emulated: `EEPROM.begin(size)` before use and `EEPROM.commit()` to persist are both mandatory on both cores; there is no `update()` — write-on-change is implemented in the app code.
- WPA2 silently degrades if the AP password is under 8 characters; `.env.example` values are intentionally ≥8 so CI builds work.

## Workflow

- For changes that touch pin assignments, boot straps, relay polarity, or the EEPROM record layout, plan the change first (read the relevant Hardware gotchas) before editing — these are destructive to get wrong on real hardware.
- Keep the split: anything testable goes in `lib/blink/` (pure, no Arduino includes), never in `src/main.cpp`. New behavior gets a `test/test_*` suite alongside it.

## Verification

Run after any change, before considering a task done:

- `mise run format` — formats `.cpp`/`.h`; CI gates on `format-check`, so an unformatted push fails CI.
- `mise run test` — host-native unit tests for the `lib/blink` modules.
- `mise run build:esp01` — confirms the firmware still compiles for `esp01_1m`.
- `mise run build:esp32` — confirms it also still compiles for `esp32dev` (the first run installs the `espressif32` platform, which takes a few minutes).

A change to `lib/blink/` or `src/main.cpp` is not done until format + test + `build:esp01` (+ `build:esp32` if `src/main.cpp` or `platformio.ini` changed) all pass.

## Deep dive (read on demand)

- [CONTEXT.md](CONTEXT.md) — domain glossary. Use these exact terms in code, comments, and the API: Hotspot (not AP/SoftAP), Lamp (not output/light), Setting (not config), Remote (not client), plus Mode/Period/Relay/Factory Default.
- [docs/adr/0001-hotspot-only-connectivity.md](docs/adr/0001-hotspot-only-connectivity.md) — why blinky is SoftAP-only (no station mode, hence no NTP/MQTT). Read before touching WiFi/connectivity; changing it reopens this decision.

## Learnings

Living section — when the user corrects a recurring mistake, append the rule here so it isn't repeated.

- _(none yet)_
