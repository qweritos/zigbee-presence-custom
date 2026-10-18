# Configuration

Endpoints 1–4 are fixed logical source endpoints. Each has an On/Off client
`0x0006`, Occupancy Sensing server `0x0406`, and settings server `0xFC10`.
Endpoint 1 also has Basic, illuminance, diagnostics and OTA clusters. The
radar profile adds `0xFC20` for settings forwarded to its original MCU.

Route settings (`0xFC10` on the source endpoint):

| Attribute | Type | Meaning |
| --- | --- | --- |
| `0x0000` | boolean | Enabled |
| `0x0001` | uint8 | Input datapoint ID, 1–255; 0 allowed while disabled |
| `0x0002` | enum8 | Tuya type: 1 boolean, 2 integer, 4 enum, 5 bitmap |
| `0x0003` | uint32 | Value that sends On |
| `0x0004` | uint32 | Value that sends Off |
| `0x0005` | uint16 | Off delay in seconds, 0–65535 |
| `0x0010` | uint32 | Last matching input value |
| `0x0011` | boolean | Input state known |
| `0x0012` | uint8 | Number of On/Off unicast bindings |
| `0x0013` | uint32 | Matching report count |
| `0x0014` | uint32 | Values ignored because they match neither configured state |

Disable a route before editing its mapping, then enable it. Writes are
validated and persisted before success is returned. Active and clear values
must differ and fit the selected Tuya type. Unrelated/unknown values cause no
action. Changing a mapping discards its old state until a fresh valid report.
An initial unknown state never sends Off. Enabling a route resynchronizes a
known input; disabling it cancels pending commands without forcing Off.

Create a **standard ZDO unicast binding** with:

- source IEEE: the bridge's factory IEEE
- source endpoint: selected route, 1–4
- cluster: `0x0006`
- destination IEEE: target actuator
- destination endpoint: the actuator's actual On/Off server endpoint

Source and destination endpoint numbers may differ. No destination is compiled
into the firmware. Binding persistence is handled by the Zigbee stack. Bind
only the On/Off client for control; occupancy reporting bindings serve telemetry.
Remove an unintended coordinator On/Off binding if your controller adds one.
Group/multicast bindings are not implemented in this initial version.

Zigbee2MQTT's [standard endpoint binding API](https://www.zigbee2mqtt.io/guide/usage/binding.html#binding-specific-endpoint)
can express the intended mapping explicitly, for example on
`zigbee2mqtt/bridge/request/device/bind`:

```json
{"from":"bridge","from_endpoint":2,"to":"actuator","to_endpoint":1,"clusters":["genOnOff"]}
```

This documents the protocol mapping; Zigbee2MQTT integration has not been
hardware-tested. The included settings UI is the ZHA quirk. Its manufacturer
and model strings must match the selected firmware profile.

For Home Assistant OTA, copy the build's `ota/` files into
`/config/ota/tuya-mcu-bridge/` and add this provider under the existing
`zha.zigpy_config.ota.extra_providers` list, preserving other providers:

```yaml
- type: zigpy_local
  index_file: /config/ota/tuya-mcu-bridge/index.json
```

Restart HA to load a changed YAML provider configuration. The index is restricted
to this firmware's manufacturer/model and exact OTA IDs. Current development
packages are unencrypted and unsigned; the development bootloader accepts them.

`0xFC11` / attribute `0x0010` on endpoint 1 configures an illuminance input DP
(0 disables it). It expects Tuya integer lux and publishes the ZCL logarithmic
illuminance measurement. All route configuration persists independently of HA.
The generic profile defaults to disabled routes and illuminance. The ZTC6
profile defaults route 1 to DP 1 enum, On=1, Off=0, delay=0, and illuminance DP104.

The ZTC6-only cluster `0xFC20` exposes motion sensitivity (2), minimum/maximum
range in cm (3/4), LED boolean (101), presence sensitivity raw 0–100 (102), and
read-only distance in cm (9, float). Writes are sent to the MCU; its reports
establish the resulting value. No extra detection/fading parameters are invented.
The initial MCU did not report DP101 after an LED write, so its LED state remains
unknown; that optional control has no confirmed hardware acceptance. The range
and sensitivity settings are read from actual MCU reports rather than guessed defaults.
