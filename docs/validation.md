# Initial hardware validation

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
| Production OTA version 2 to version 3, diagnostic mailbox omitted | In progress |
| Optional MCU LED state/control confirmation | Unconfirmed; MCU did not report DP101 |

Input transitions for the control tests were injected through the SWD diagnostic
interface. A separate parser kept that traffic independent of live UART input.
Actual MCU DP1 reports were also decoded and observed driving the relay after
the real input mapping was restored. Actuator state was read from the actual
Zigbee endpoint; visual lamp state and a person walking into/out of a room were
not the test method.

The replaced HA automation was disabled throughout acceptance tests. During
the outage test, the HA application could not execute and its HTTP API timed
out. State queries originated on the sensor and were read through SWD. The
coordinator radio remained powered; coordinator-hardware power-loss recovery
is outside this result.

Development OTA packages are unsigned and unencrypted. The build uses an
external Silicon Labs SDK and records the SLC compatibility override. These
results cover the initial profile and hardware, not every Tuya MCU product.
