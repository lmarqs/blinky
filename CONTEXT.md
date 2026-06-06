# Blinky

A self-contained WiFi device that blinks a lamp through a relay. It is its own
hotspot; a remote device joins it to change the blinking behavior.

## Language

**Factory Default**:
The behavior when no valid Setting exists (first boot, corrupted or
outdated record): Blink at a 5-second Period.
_Avoid_: default (alone), initial state

**Hotspot**:
The WPA2 WiFi network blinky broadcasts. The only way to reach the device —
it never joins another network.
_Avoid_: AP, SoftAP, access point

**Lamp**:
The appliance being controlled. Status reports the intended lamp state
(energized or dark).
_Avoid_: output, light, load

**Setting**:
The persisted behavior — Mode and Period as one atomic record. Restored
all-or-nothing at boot; survives power cuts.
_Avoid_: settings, config, preferences

**Remote**:
Any device joined to the Hotspot operating the lamp through the UI or API.
_Avoid_: client, controller, phone

**Relay**:
The mechanical switch that powers the lamp. Each toggle spends one cycle of
its finite life and makes an audible click.
_Avoid_: switch

**Mode**:
The lamp's behavior selector: Blink, On, or Off. The three are peers — what
you set is what survives a power cut.
_Avoid_: state, override

**Period**:
The time the lamp holds each state before toggling. A full blink cycle is two
periods (on + off).
_Avoid_: interval, cycle, frequency
