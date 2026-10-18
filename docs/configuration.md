# Configuration

Endpoint 1 has an On/Off client
`0x0006`, Occupancy Sensing server `0x0406`, and settings server `0xFC10`.
Endpoint 1 also has Basic, illuminance, diagnostics and OTA clusters. The
radar profile adds `0xFC20` for settings forwarded to its original MCU.

Control settings (`0xFC10` on endpoint 1):

| Attribute | Type | Meaning |
| --- | --- | --- |
| `0x0000` | boolean | Enabled |
| `0x0001` | uint8 | Input datapoint ID, 1–255; 0 allowed while disabled |
| `0x0002` | enum8 | Tuya type: 1 boolean, 2 integer, 4 enum, 5 bitmap |
| `0x0003` | uint32 | Value that sends On |
| `0x0004` | uint32 | Value that sends Off |
| `0x0005` | uint16 | Off delay in seconds, 0–65535 |
| `0x0006` | uint16 | On illuminance threshold, 0–65535 lux; 0 disables the limit |
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

The On illuminance threshold defaults to **0**, which permits On without a lux
report. A nonzero limit permits On only when a known lux reading is at or below
the value. While occupied and blocked, a later qualifying lux report can permit
On. Once accepted, On stays latched until occupancy clears and any Off delay
expires; brightness from the lamp cannot turn it off. Renewed occupancy during
the delay retains On. A nonzero limit with unknown lux waits for a valid report.
The limit affects On only; it never suppresses occupancy-driven Off. The supported model selects its illuminance datapoint automatically.

Create a **standard ZDO unicast binding** with:

- source IEEE: the bridge's factory IEEE
- source endpoint: 1
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
{"from":"bridge","from_endpoint":1,"to":"actuator","to_endpoint":1,"clusters":["genOnOff"]}
```

This documents the protocol mapping; Zigbee2MQTT integration has not been
hardware-tested. The included settings UI is the ZHA quirk. Its manufacturer
and model strings must match the selected firmware profile.

For Home Assistant OTA, copy the build's `ota/` files into
`/config/ota/zigbee-presence/` and add this provider under the existing
`zha.zigpy_config.ota.extra_providers` list, preserving other providers:

```yaml
- type: zigpy_local
  index_file: /config/ota/zigbee-presence/index.json
```

Restart HA to load a changed YAML provider configuration. The index is restricted
to this firmware's manufacturer/model and exact OTA IDs. Current development
packages are unencrypted and unsigned; the development bootloader accepts them.

`0xFC11` / attribute `0x0010` on endpoint 1 configures an illuminance input DP
(0 disables it). It expects Tuya integer lux and publishes the ZCL logarithmic
illuminance measurement. All route configuration persists independently of HA.
The supported `zy-m100` profile defaults endpoint 1 to DP 1 enum, On=1, Off=0, delay=0, and illuminance DP104.

The ZY-M100 cluster `0xFC20` exposes motion sensitivity (2), minimum/maximum
range in cm (3/4), presence sensitivity raw 0–100 (102), and
read-only distance in cm (9, float). Writes are sent to the MCU; its reports
establish the resulting value. No extra detection/fading parameters are invented.
The unsupported optional LED setting is omitted. Defaults are 8 for motion
sensitivity, 0/525 cm for minimum/maximum range, and 100 raw (10/10) for
presence sensitivity. Desired settings persist in radio NVM and are reapplied
at startup, with bounded retries; only MCU reports establish actual values.
Unconfirmed readings remain unknown rather than displaying invented values.
The normal HA UI exposes radar settings, off delay and
On illuminance threshold. Direct control defaults to enabled; its advanced
master-enable attribute is omitted from HA controls. Protocol datapoint IDs/types/active/clear values are
advanced ZCL attributes, not HA controls.

Illuminance DP104 is already a lux value from the unchanged radar MCU. Its ZCL
encoding is `0` at zero lux, otherwise `min(65534, 10000*log10(lux)+1)`.
This matches the original ZHA converter; 1000 lux encodes to 30001. A displayed
reading near 1000 lux is not a new factor-of-ten conversion in this firmware.

Pairing uses the existing MCU button and LED. Hold the button for about five
seconds: the stock MCU requests leave-and-join (UART command3, payload1).
The radio acknowledges it and advertises pairing status3 throughout a
180-second commissioning window, including between scan retries, then status1
after joining or status0 on timeout. Network/configuration storage is preserved
by normal firmware updates; pairing intentionally changes network membership.
See [validation](validation.md) for the distinction between stock MCU emulation,
radio commissioning tests and physical button/LED observations.

Upgrading from the earlier four-endpoint firmware preserves endpoint 1 settings
and bindings, defaults its new lux limit to 0, and deletes retired endpoints 2–4
settings and bindings. Reinterview the device after upgrading and remove the
retired entities from the HA registry. The current quirk exposes only endpoint 1.

After changing the device profile/quirk or re-pairing an existing HA device,
run **Reconfigure** in ZHA to restore coordinator attribute reporting. A fresh
Occupancy Sensing read can update HA once even when unsolicited reports are
missing. Verify On and Off updates through HA's cached entity state. Standard
occupancy/illuminance reporting bindings are separate from the peer On/Off
control binding and require no HA control automation.
