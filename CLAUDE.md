# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Firmware for an ESP-01S (ESP8266) socketed in a relay carrier board that blinks a mains lamp. The device is a self-contained WiFi hotspot (SoftAP only — it never joins a network); a phone joins the `blinky` AP and controls it at `http://192.168.4.1`. Default behavior: blink every 5s. Mode (`blink|on|off`) and period are settable over HTTP and persist to EEPROM.

## Commands

Everything goes through mise (it loads `.env` and provides the python venv with PlatformIO — bare `pio` invocations outside mise will miss the env vars that `platformio.ini` injects via `${sysenv.*}`):

- `mise run setup` — create `.env` from `.env.example`, install PlatformIO into `.venv` from `requirements.txt`
- `mise run test` — host-native unit tests (`pio test -e native`); single suite: `pio test -e native -f test_blink`
- `mise run build` — compile firmware (`esp01_1m` env)
- `mise run upload` — wired flash via CH340 (chip must be **out** of the relay carrier and socketed in the CH340 adapter)
- `mise run ota` — flash over WiFi via espota to `192.168.4.1`; **the laptop must be joined to the blinky AP**
- `mise run monitor` — serial monitor at 115200
- `mise run hexdump` — regenerate C headers from `assets/` web assets (runs automatically before build/upload/ota)

## Architecture

- `lib/blink/BlinkController.{h,cpp}` — pure state machine, **no Arduino includes**. The clock is passed into `update(nowMs)`, which makes it host-testable and millis()-rollover-safe (unsigned subtraction idiom). All behavior logic (modes, period clamping 200ms–1h, toggle timing) lives here.
- `src/main.cpp` — device glue only: SoftAP, sync `ESP8266WebServer` routes (`GET /`, `GET /status`, `POST /mode`, `POST /period`), ArduinoOTA, EEPROM persistence (magic/version struct, write-on-change), relay pin driving.
- `assets/index.html` — the web UI source. The `hexdump` mise task converts every non-`.h` file in `assets/` into a `const ... PROGMEM` byte-array header next to it (e.g. `assets/index.html.h`, symbols `index_html` / `index_html_len`). Generated headers are gitignored; edit the `.html`, never the `.h`. `platformio.ini` adds `-I assets` so `src/main.cpp` includes it directly.
- `test/test_blink/test_main.cpp` — Unity tests, run only in the `native` env (`test_ignore = *` on embedded envs, `test_build_src = no` on native keeps Arduino-dependent `src/` out of host builds).
- Keep this split: anything testable goes in `lib/blink/`, never in `src/main.cpp`.

Two upload envs in `platformio.ini`: `esp01_1m` (wired, esptool) and `esp01_1m_ota` (espota with `--auth`). Secrets (`BLINKY_AP_SSID`, `BLINKY_AP_PASSWORD`, `BLINKY_OTA_PASSWORD`) come from gitignored `.env` as compile-time string defines — the whole build flag is single-quoted (`'-D X="${sysenv.X}"'`), which is required for string defines to survive.

## Hardware gotchas

- **GPIO0 is both the relay pin and a boot strap** (LOW at reset = flash mode). `setup()` claims the pin and de-energizes the relay first thing. `RELAY_ACTIVE_LEVEL` in `src/main.cpp` is a compile-time constant — carrier boards vary; flip it if the relay logic is inverted on real hardware.
- Wired flashing requires the chip out of the relay carrier — that's why OTA exists; prefer `mise run ota` after the first flash.
- The ESP8266 boot ROM prints its reset banner at **74880 baud** (`pio device monitor -b 74880` to read boot diagnostics); the app `Serial` runs at 115200.
- ESP8266 EEPROM is flash-emulated: `EEPROM.begin(size)` before use and `EEPROM.commit()` to persist are both mandatory; there is no `update()` — write-on-change is implemented in the app code.
- WPA2 silently degrades if the AP password is under 8 characters; `.env.example` values are intentionally ≥8 so CI builds work.
