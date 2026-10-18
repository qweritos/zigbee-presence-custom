# Releases

Download the ZIP for your model from [GitHub Releases](https://github.com/qweritos/zigbee-presence-custom/releases).
Currently only ZY-M100 v1.3, ZS3L / EFR32MG21A020F768IM32 is supported.

Each release has only:

- **Model ZIP:** wired-install application and bootloader, RAM-only SWD loader,
  ZHA quirk, instructions, manifest and license notices.
- **Model `.ota`:** updates an existing compatible custom installation.
- **`index.json`:** OTA provider index for all supported models.
- **`SHA256SUMS`:** checksums for the downloads.

Verify checksums before use. Inside the ZIP, the application `.bin` starts at
`0x4000` and the bootloader at `0`. The loader runs in RAM only.
Debug ELF and intermediate GBL files remain build outputs, not release assets.

First conversion from stock requires [wired flashing and private backups](hardware.md).
For a compatible custom installation, use the `.ota` file or add the release's
`index.json` URL as a Zigpy OTA index. The index restricts each image to its
manufacturer/model. Updates preserve network and configuration storage.
Do not initialize NVM or flash the bootloader during an ordinary OTA update.

## Maintainers

`config/models.json` lists supported profiles. CI derives its build matrix from
that file and builds every listed profile with pinned, SHA256-verified tools.
Each profile must also have a validated build configuration and hardware support;
adding a name alone does not establish compatibility.

Set `VERSION` to `MAJOR.MINOR.PATCH`, commit, then push a signed matching tag:

```
git tag -s v0.0.1 -m 'Release 0.0.1'
git push origin v0.0.1
```

Tags must match `VERSION`. Each field fits eight bits. OTA versions encode
`major << 24 | minor << 16 | patch << 8`, reserving the low byte for builds.
Thus 0.0.1 is 256, above the initial development versions 1–9.
Basic SW Build ID reports the semantic version; ApplicationProperties and the
OTA header use the same encoded integer, checked before publishing.

Main/PR builds run host tests, compile production firmware and bootloader,
run the actual SDK OTA-size regression, and upload per-model artifacts.
A successful version-tag build also verifies all model packages, creates a
combined OTA index/checksums and publishes the GitHub release.
No workflow flashes hardware. GPG-signed commits/tags do not make the unsigned
development OTA format cryptographically signed.
