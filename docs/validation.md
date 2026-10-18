# Hardware validation

## Single-endpoint illuminance revision (v7)

The current firmware exposes only source endpoint 1. The On illuminance threshold
defaults to 0 (bypass); nonzero values block On only above the configured lux.
Once On is accepted, later brightness cannot turn it off. The original EP1
configuration migrates with a zero limit; retired endpoints 2–4 configuration and
bindings are removed.

Host ASan/UBSan tests pass for above/equal/below-threshold input, unknown lux,
zero bypass, feedback latching, delayed-Off cancellation, mapping/enable changes,
and version 1/version 2 settings decoding. Both generic and radar production
profiles build as v7 with one generated endpoint.

The attempted v3→v5 OTA reached its complete image but failed when the SDK used
the old cached image size and requested beyond EOF. v5 was not installed.
The build now applies a guarded local SDK patch, preserving the external SDK.
A regression compiles the actual SDK query function against storage fixtures:
the original fails shrinking/growing/partial-cache cases; the patched copy
passes those and the intentionally reused-cache case.

| Check | Result |
| --- | --- |
| v6 diagnostic wired install and targeted page readback | PASS, 40 pages; failed staging invalidated, network NVM retained |
| v1 settings migration and original EP1 peer binding | PASS, new limit defaults to 0; no persistence errors |
| Actual descriptor and HA cleanup | PASS, one source endpoint; 48 retired/unsupported registry entries removed |
| Reset with HA unavailable and nonzero limit | PASS, settings/source/binding retained; unknown lux blocks On |
| Above/equal/below threshold with HA unavailable | PASS, direct sensor-originated actuator reads |
| Brightness feedback and cancelled delayed Off with HA unavailable | PASS |
| v6→v7 production OTA, diagnostic mailbox removed | PASS, live OTA cluster version 7 |
| Nonzero illuminance limit survives OTA | PASS, 65535 retained then restored to requested 0 |
| Production real-MCU reports | PASS, above/equal/below threshold, zero bypass, unknown lux, brightness latch, deduplication and clear-delay cancellation |
| Final restored settings | PASS, real occupancy DP1 enum 1/0, lux DP104, limit/delay 0, original sensitivity 8; actual occupancy and relay state agree |
| Final HA entities and control binding | PASS, one source endpoint, one peer binding; retired entries did not reappear |
| Complete production app and factory-region readback | PASS, exact application match; USERDATA/DEVINFO unchanged, original IEEE and CTUNE160 preserved |

The v6 outage test used SWD input injection and suspended both HA and its
supervisor Python processes; process states and HTTP timeout were checked.
Actuator reads originated on the sensor. A bounded retry allows address/route
discovery after reset. This result does not claim a physical walk-through.

Production tests temporarily used integer MCU sensitivity reports as the input
and illuminance source, forwarding real commands to the MCU and reading the
actual relay after its reports. Original settings and real occupancy/lux inputs
were restored afterward. The former HA automation stayed disabled.
HA polling encountered timeouts with short observation windows; the final
suite allows 20 seconds for transport observations. An isolated trace showed
the configured two-second Off, and the independent outage test also passed
delay/cancellation. No firmware timing was changed for HA polling latency.

The v7 production application is 286160 bytes; the complete installed readback
has SHA256 `531d7b0b51f27ed31cef09a6b626cebcf5966cf9d064da5c6c1e13ef325bb295`.

## Initial v3 deployment

Target: ZY-M100-v1.3, `_TZE204_ztc6ggyl`, ZS3L with
EFR32MG21A020F768IM32 (768 KiB flash, 64 KiB RAM). Target actuator:
eWeLink ZB-SW01, On/Off server on endpoint 1. Personal device addresses,
network credentials, stock firmware and detailed deployment logs are private.

| Check | Result |
| --- | --- |
| Parser framing, checksum, corruption recovery, truncated-frame expiry and fuzz input | PASS, host ASan/UBSan |
| Independent route mappings, persistence encoding, validation, delay/cancel, retries and clock rollover | PASS, host ASan/UBSan |
| GBL/OTA format, CRC and version/vector/layout rejection guards | PASS, host tests and independent Zigpy OTA parser |
| First wired conversion | PASS, 42 targeted pages matched readback |
| Native MCU occupancy, lux, range and sensitivity reports | PASS |
| Source endpoint 1 controls actuator endpoint 1 | PASS, fresh actuator-state reads |
| Source endpoint 2 controls actuator endpoint 1 | PASS, including read-response endpoint verification |
| Unchanged reports do not resend commands | PASS |
| Local delayed Off, cancellation and disabling a pending route | PASS |
| Repeated cancellation checks | PASS, including ten cycles using direct Zigbee reads |
| Route configuration and bindings survive reset | PASS |
| Unknown input after reset does not force Off | PASS |
| Control with HA unavailable | PASS, HA Python process and supervisor watchdog suspended; process state and API timeout checked |
| Reset, three On/Off cycles and delayed-Off cancellation while HA unavailable | PASS |
| Actual OTA version 1 to version 2 | PASS; new firmware executed and answered direct actuator-state queries |
| Production OTA version 2 to version 3, diagnostic mailbox omitted | PASS; installed version and complete application readback match |
| Production routes real MCU integer reports to the actuator | PASS, two complete On/Off cycles; original MCU sensitivity restored |
| ZHA reconfiguration preserves exact peer control bindings | PASS, one source EP1 to actuator EP1 binding; EP2–4 have none |
| Final production reset and restored occupancy mapping | PASS, fresh MCU DP1 report and matching actuator state; extra routes disabled |
| Factory USERDATA and DEVINFO | PASS, unchanged against verified stock backups; original IEEE retained |
| Stock HFXO calibration | PASS, CTUNE160 read back live |
| Optional MCU LED state/control confirmation | Unconfirmed; MCU did not report DP101 |

Input transitions for the control tests were injected through the SWD diagnostic
interface. A separate parser kept that traffic independent of live UART input.
Actual MCU DP1 reports were also decoded and observed driving the relay after
the real input mapping was restored. Actuator state was read from the actual
Zigbee endpoint; visual lamp state and a person walking into/out of a room were
not the test method.

The production build has no diagnostic injection mailbox. Its additional live
test temporarily mapped actual MCU sensitivity reports to On/Off, changed the
sensitivity twice in each direction, and verified the actuator after each MCU
report. The original sensitivity and DP1 occupancy mapping were then restored.
After the final radio-only reset, a valid occupancy sample arrived later than
the initial 30-second check. Until such a report, the bridge deliberately keeps
the input unknown and sends no forced Off. The final fresh sample and actuator
state agreed.

The installed production application is 286660 bytes; its SHA256 is
`fce22a0e1cb42b665e4b4ade4844576367b035b13197d60665697d49848648a9`.
Both generic and initial radar profiles build as version 3. The deployed profile
is the radar variant; the generic profile has not been installed on another
product as part of this acceptance.

The replaced HA automation was disabled throughout acceptance tests. During
the outage test, the HA application could not execute and its HTTP API timed
out. State queries originated on the sensor and were read through SWD. The
coordinator radio remained powered; coordinator-hardware power-loss recovery
is outside this result.

Development OTA packages are unsigned and unencrypted. The build uses an
external Silicon Labs SDK and records the SLC compatibility override. These
results cover the initial profile and hardware, not every Tuya MCU product.
