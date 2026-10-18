# Hardware validation

## Presence firmware naming, defaults and pairing

The project is now `zigbee-presence-custom`; the supported models table lists
only ZY-M100. The ZY-M100 build uses manufacturer `www.andrey.wtf`, model
`ZY-M100`; the factory hardware IEEE remains unchanged. The only supported
build profile is `zy-m100`. Earlier generic build results below are historical
and do not claim support for another product.

Host tests cover explicit radar defaults, versioned persistence decoding,
invalid ranges and the original logarithmic ZCL illuminance conversion.
The unchanged MCU supplies integer lux. Historical stock HA data before the
first custom flash already included values around 1000 lux. A direct live read
of production v7 showed raw MCU 1006 and ZCL 30026 (HA 1005.77 lx), consistent with
`10000*log10(lux)+1` and the original ZHA quirk. No arbitrary rescaling is applied.

The separate MCU owns the physical button and LED. Its matching original image
[`ZY-M100_L_TZE204_ztc6ggyl.bin`](https://github.com/Andrik45719/ZY-M100/blob/03770f2873119606d88cecb19a2432fca7c2817e/ZY-M100_L_TZE204_ztc6ggyl.bin)
has SHA256 `946831e660267822dde10ec9f02a7486711a90de2b898ef77d7b06737a5f389b`.
The live MCU reports version 0x42, matching that image's version routine; an MCU
readback has not established that the installed image is byte-identical.
`scripts/check_stock_pairing.py ORIGINAL.bin` (requires Unicorn) emulates the
hash-identified binary's routines: 5000 ms does not trigger, 5001 ms sends UART
command 3 / payload 1 once per hold; release rearms it. With the LED routine enabled,
network state 3 produces GPIO PA0 edges at 0, 502, 1001, 1503 ms (about 500 ms intervals).
This is binary-logic evidence, a simulation; physical acceptance is recorded below.

The radio handles that existing MCU request and keeps pairing network status 3
throughout the 180-second commissioning window, including gaps between scans.
It also accepts MCU query responses using command 0x2C. Normal configuration UI
omits datapoint IDs/types/values and the optional unconfirmed LED control.

Diagnostic revision v8 was installed through J-Link with all 40 written pages
verified. At 3.6 seconds after boot the real MCU had confirmed all four configured
radar defaults (8,0,525,100). A user setting 7, lux limit 77 and delay 2 survived a
radio reset; actual MCU reports confirmed 7 after restart, and the original
endpoint 1 relay binding remained. Settings were restored afterward.

SWD injected the completed MCU pairing request (command 3 / payload 1) into the
radio frame handler. State 3 and a real MCU status ACK appeared at 1.09 seconds;
the radio rejoined and the MCU acknowledged state 1 at 6.81 seconds. This validates
the radio/MCU status exchange and commissioning path, while the physical button
and optical observation is recorded separately below. Fresh Basic reads and HA reinterview
confirmed manufacturer/model and applied the new quirk to the existing device
identity. Six retired protocol/LED controls were removed from the registry;
the master Direct control enabled toggle was subsequently removed on request.
a transient unsupported On/Off-client opening entity was also removed.

Physical acceptance was subsequently captured on the running production v9:
a six-second button hold produced a valid UART-v2 command 3 / payload 01 at 25.44 s
of the capture, a real MCU network-status ACK at 26.37 s, and network status 3
at 26.38 s. The user confirmed that the LED was blinking. This establishes the
physical button-to-radio request and visible pairing indication, beyond the
earlier stock-binary emulation. Earlier five-second attempts did not provide
that observation; the stock timer threshold is 5001 ms, so allow about six seconds.
After a full power cycle, SWD also found the production application running,
MCU version 0x42 and all four confirmed radar defaults; firmware is in internal
flash and survives removal of power. A later missing HA device entry required
recommissioning; it was not evidence that application flash had been erased.


Production v9 is installed through J-Link. The initial attempt stopped on an
SWD parity/readback error and was not accepted. Recovery used the verified
pre-update v8 snapshot, backed up each current target page, skipped matching
pages and verified all 39 pages. A complete application readback matched all
286628 bytes, SHA256 `9d863eec806f77faa55515153674f7c30ea994308ec65546d88f67b9ca9b2924`.
Factory USERDATA/DEVINFO remained byte-identical to the original backups.
The production ELF contains no diagnostic mailbox.

The final real-MCU/relay suite passed all eight checks: installed version and
binding, above/equal/below lux gating, brightness feedback latch, duplicate
suppression, zero bypass, unknown lux, delayed-Off cancellation, and restoration
of the original occupancy/lux sources and radar defaults. The test primes a
changed sensitivity first because writing an already confirmed value no longer
forces a duplicate MCU report. HA stayed free of a control automation.
After HA restart, the requested manufacturer/model and quirk still match,
24 device registry entries remain, and removed technical controls did not
reappear. Live ZHA reports Router; the build has six end-device child slots.

After physical pairing/rejoin, fresh device reads confirmed known occupancy 1
and the bound relay On, exactly one endpoint 1 On/Off binding, enabled control,
DP1 enum 1/0, DP104 illuminance, zero threshold/delay and radar defaults 8/0/525/100.
No HA control automation was enabled. NVM failures and RX-ring overflow were 0;
the debug-session UART error counter was 1. UI limits were verified as 0–9.5 m
with 0.01 m steps and presence sensitivity 0–10 with 0.1 steps. The hidden master
toggle stayed removed while the firmware's enable attribute remained true.


After recommissioning, HA displayed detected while the MCU had sent clear and
a fresh Zigbee Occupancy Sensing read returned 0. That read immediately cleared
HA's cached state. ZHA Reconfigure restored coordinator reporting. A separate
real-MCU sensitivity-input test then observed HA Off→On→Off through REST cached
entity state alone, with no occupancy attribute reads. The same unsolicited
reporting test passed again after a radio reset. The user's current sensitivity 3
and other settings were preserved; original DP1 mapping, delay and lux limit
were restored. No firmware polling or HA control automation was added.


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
