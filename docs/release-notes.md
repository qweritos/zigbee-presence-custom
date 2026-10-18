First public firmware release for **ZY-M100 v1.3, ZS3L / EFR32MG21A020F768IM32**.

- Lux-gated direct Zigbee On/Off binding, with brightness latch and optional off delay.
- Configurable, persistent radar sensitivity and range; HA configuration via the included ZHA quirk.
- Router operation and OTA updates after the initial wired conversion.

Use the model ZIP for first installation, `.ota` for an existing compatible custom installation, and `index.json` for a Zigpy OTA provider. Checksums and build manifests accompany the artifacts.

Version **0.0.1** uses OTA file version **0x00000100 (256)**, newer than development versions 1–9. Firmware and OTA header versions match. Release builds omit the SWD diagnostic mailbox.

First stock conversion requires SWD and verified private backups. Do not install on unverified board variants. Subsequent OTA updates retain network/settings; do not initialize NVM on an update.

HA outage and real-MCU/direct-relay operation were validated during development. Coordinator hardware power-loss operation remains untested. These release binaries are CI-built; publishing does not flash a device or claim a new hardware acceptance run. OTA images are unsigned and unencrypted; GPG commit/tag verification is separate from firmware signing.
