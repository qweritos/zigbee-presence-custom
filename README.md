# Tuya MCU Zigbee bridge

Configurable radio firmware for Tuya MCU devices using the ZS3L / EFR32MG21.
Map MCU datapoints to standard Zigbee On/Off commands and bind the
source endpoint directly to destination devices and endpoints. Control runs
on the radio, including its optional off delay, without Home Assistant automations.

**Experimental:** direct control, timers, persistence and operation with HA
suspended have passed on the initial hardware profile. Production version 7
is installed via OTA; real MCU reports passed illuminance-gated relay control.
The factory identity and original endpoint 1 binding are preserved.
See the precise methods and limits in [validation](docs/validation.md).

One configurable input supports boolean, enum, unsigned integer or bitmap
values, active/clear values, enable, off delay, and an On illuminance threshold.
Endpoint 1 exposes an On/Off client, occupancy server and control settings. The standard
binding table stores destination IEEE addresses and endpoint numbers. Multiple
destinations can be bound to the source endpoint. Commands are absolute On/Off;
unchanged reports do not repeatedly override a manually changed light. The
On illuminance threshold defaults to 0 (no limit); a nonzero value blocks On
above the chosen lux level. An accepted On remains on until occupancy clears.

The generic profile starts with direct control disabled. The `ztc6ggyl` profile adds
defaults and settings for the ZY-M100-v1.3 `_TZE204_ztc6ggyl` radar. It preserves
the separate MCU firmware and does not overwrite its settings at startup.
This is a reusable source project, not a universal binary for every Tuya device.

Run `make test` for the portable parser and routing tests. See [build instructions](docs/build.md),
[configuration and binding](docs/configuration.md), and [hardware/recovery](docs/hardware.md).
The [ZHA quirk](quirks/tuya_mcu_bridge.py) exposes settings; it does not implement
the control logic. Standard Zigbee management can configure the firmware without it.

Application source is distributed under the included [MIT license](LICENSE). See [NOTICE](NOTICE)
for upstream attribution and external SDK licensing. Stock dumps, credentials,
SDK archives, and compiled device images are intentionally outside the source tree.
Current development OTA packages are unsigned and unencrypted.
