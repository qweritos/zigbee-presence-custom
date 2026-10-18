# Tuya MCU Zigbee bridge

Configurable radio firmware for Tuya MCU devices using the ZS3L / EFR32MG21.
Map MCU datapoints to standard Zigbee On/Off commands and bind each logical
source endpoint directly to destination devices and endpoints. Control runs
on the radio, including its optional off delay, without Home Assistant automations.

**Experimental:** direct control, timers, independent endpoints, persistence and
operation with HA suspended have passed on the initial hardware profile. Actual
OTA updates 1 → 2 → 3 passed; production version 3 is installed and its complete
application readback matches the build. The factory identity is preserved.
See the precise methods and limits in [validation](docs/validation.md).

Four independent routes support boolean, enum, unsigned integer or bitmap
inputs, configurable active/clear values, enable and off delay. Endpoints 1–4
each expose an On/Off client, occupancy server and route settings. The standard
binding table stores destination IEEE addresses and endpoint numbers. Multiple
destinations can be bound to a source endpoint. Commands are absolute On/Off;
unchanged reports do not repeatedly override a manually changed light.

The generic profile starts with routes disabled. The `ztc6ggyl` profile adds
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
