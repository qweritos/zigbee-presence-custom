<div align="center">
  <h1><img alt="Zigbee" height="48" src="https://cdn.simpleicons.org/zigbee/EB0443" /> zigbee-presence-custom</h1>
  <p>Custom firmware for Zigbee radar presence sensors</p>
  <p>Lux-gated direct binding keeps lighting working through Home Assistant and automation downtime.</p>
</div>

<p align="center">
  <a href="https://github.com/qweritos/zigbee-presence-custom/actions"><img alt="Checks" src="https://img.shields.io/github/actions/workflow/status/qweritos/zigbee-presence-custom/host-tests.yml?style=flat-square" /></a>
  <a href="LICENSE"><img alt="License" src="https://img.shields.io/github/license/qweritos/zigbee-presence-custom?style=flat-square" /></a>
  <a href="https://github.com/qweritos/zigbee-presence-custom/stargazers"><img alt="Stars" src="https://img.shields.io/github/stars/qweritos/zigbee-presence-custom?style=flat-square" /></a>
  <a href="https://github.com/qweritos/zigbee-presence-custom/issues"><img alt="Issues" src="https://img.shields.io/github/issues/qweritos/zigbee-presence-custom?style=flat-square" /></a>
</p>

- **Local control:** occupancy sends On/Off directly to bound Zigbee devices and endpoints; no HA automations required.
- **Lux gating:** turn on only below your threshold, then stay on until occupancy clears. Threshold `0` always permits On.
- **Configurable:** radar sensitivity, range and off delay; settings survive power cycles.
- **Zigbee router:** relays mesh traffic and can parent up to six end devices.
- **Home Assistant:** configuration and telemetry through the included [ZHA quirk](quirks/zigbee_presence.py); OTA updates after the first wired flash.

Control can also survive coordinator downtime when powered peers retain a working mesh path.
HA outage operation is tested; coordinator power-loss operation remains untested.

## Supported models

| Model | Tested hardware | Photo |
| --- | --- | --- |
| **ZY-M100** | v1.3, original `_TZE204_ztc6ggyl`, ZS3L / EFR32MG21 | <a href="docs/images/zy-m100-zs3l-jlink.jpg"><img src="docs/images/zy-m100-zs3l-jlink-thumb.jpg" alt="Opened ZY-M100 with ZS3L module and J-Link connected; click to enlarge" width="180" height="240" /></a> |

Other models and board variants need separate validation. The factory IEEE and original radar MCU are retained.

## Get started

[Build and test](docs/build.md) · [Flash and recover](docs/hardware.md) · [Configure and bind](docs/configuration.md) · [Validation](docs/validation.md)

Experimental firmware. Current OTA images are unsigned and unencrypted.
[MIT license](LICENSE) · [Upstream attribution and SDK licensing](NOTICE)
