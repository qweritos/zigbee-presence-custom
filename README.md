# Zigbee presence firmware

Configurable Zigbee presence sensor firmware for the ZS3L / EFR32MG21.
Map MCU datapoints to standard Zigbee On/Off commands and bind the
source endpoint directly to destination devices and endpoints. Control runs
on the radio, including its optional off delay, without Home Assistant automations.

Supported models:

| Model | Tested hardware | Zigbee manufacturer | Zigbee model |
| --- | --- | --- | --- |
| **ZY-M100** | v1.3, original `_TZE204_ztc6ggyl`, ZS3L / EFR32MG21 | `www.andrey.wtf` | `ZY-M100` |

This list currently contains only ZY-M100. Other products and ZY-M100 board
variants require separate validation. The radio retains the factory IEEE and
uses its original separate radar MCU.

**Experimental:** direct control, illuminance gating, timers and persistence
have passed real MCU/relay tests, including operation with HA suspended.
See the methods and limits in [validation](docs/validation.md).

One configurable input supports boolean, enum, unsigned integer or bitmap
values, active/clear values, enable, off delay, and an On illuminance threshold.
Endpoint 1 exposes an On/Off client, occupancy server and control settings. The standard
binding table stores destination IEEE addresses and endpoint numbers. Multiple
destinations can be bound to the source endpoint. Commands are absolute On/Off;
unchanged reports do not repeatedly override a manually changed light. The
On illuminance threshold defaults to 0 (no limit); a nonzero value blocks On
above the chosen lux level. An accepted On remains on until occupancy clears.

The device runs as a Zigbee router and can parent up to six end devices with
this build, in addition to relaying mesh traffic. It requires continuous power.

The `zy-m100` profile starts with direct control enabled, zero off delay and
zero illuminance threshold (always permit On). Radar defaults are motion
sensitivity 8, range 0–5.25 m and presence sensitivity 10/10. Settings are
persisted and applied to the MCU at startup; readings become valid only after
MCU confirmation. Datapoint mapping is set by the model profile and omitted
from the normal HA settings UI. Advanced ZCL mapping remains available for
source development and testing. Standard endpoint bindings choose the actuator.

Run `make test` for the portable parser and routing tests. See [build instructions](docs/build.md),
[configuration and binding](docs/configuration.md), and [hardware/recovery](docs/hardware.md).
The [ZHA quirk](quirks/zigbee_presence.py) exposes settings; it does not implement
the control logic. Standard Zigbee management can configure the firmware without it.

Application source is distributed under the included [MIT license](LICENSE). See [NOTICE](NOTICE)
for upstream attribution and external SDK licensing. Stock dumps, credentials,
SDK archives, and compiled device images are intentionally outside the source tree.
Current development OTA packages are unsigned and unencrypted.
