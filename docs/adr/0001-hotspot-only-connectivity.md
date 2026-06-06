# Hotspot-only connectivity

Blinky never joins a WiFi network — it only broadcasts its own WPA2 Hotspot,
and the Remote comes to it (`192.168.4.1`). Chosen over station/hybrid mode to
eliminate credential management entirely (the repo is public; home WiFi
credentials would need provisioning, storage, and a reset story) and to keep
the device fully self-contained: it works anywhere there is 5V.

## Consequences

- No internet for the device, ever: no NTP (so no wall-clock schedules — only
  relative timing), no MQTT/Home Assistant integration. Revisiting any of
  these reopens this decision, including provisioning.
- The Remote loses its own internet connection while joined to the Hotspot —
  acceptable for occasional reconfiguration, wrong for frequent control.
- OTA updates require the flashing machine to join the Hotspot first.
- The Hotspot password is the device's entire security boundary (radio range);
  it is injected at compile time from a gitignored `.env`.
