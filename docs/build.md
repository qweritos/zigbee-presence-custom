# Build

External Linux x86_64 dependencies used during development:

| Tool | Version | Archive SHA256 |
| --- | --- | --- |
| [Silicon Labs Simplicity SDK](https://github.com/SiliconLabs/simplicity_sdk/releases/download/v2025.6.2/simplicity-sdk.zip) | 2025.6.2 | `463021f42ab1b4eeb1ca69660d3e8fccda4db76bd95b9cccec2c2bcba87550de` |
| [Arm GNU Toolchain](https://developer.arm.com/-/media/Files/downloads/gnu/12.3.rel1/binrel/arm-gnu-toolchain-12.3.rel1-x86_64-arm-none-eabi.tar.xz) | 12.3.Rel1 | `12a2815644318ebcceaf84beabb665d0924b6e79e21048452c5331a56332b309` |
| [ZAP](https://github.com/project-chip/zap/releases/download/v2025.10.23/zap-linux-x64.zip) | 2025.10.23 | `bf7d4b388ee6b82ef5813fd9a2ed19c8b18039195d708add6122a5377a152d22` |
| [SLC CLI (Edge Impulse mirror)](https://cdn.edgeimpulse.com/build-system/slc_cli_linux.zip) | 5.7.0.0 | `eb2a0bec554e394b0278b702ee9d8f63b43f44ee2e416af72bf32215ce9d436d` |

Install Java 21, make, Python 3, and a host C compiler. Extract external tools
into a directory with `sdk/`, `gcc/arm-gnu-toolchain-12.3.rel1-x86_64-arm-none-eabi/`,
`zap/zap-cli`, and `slc/slc_cli/slc`. Set `JAVA_HOME` to Java 21.

```
make test
python3 scripts/build.py --tools /path/to/tools --profile generic --version 3 --force-generation
python3 scripts/build.py --tools /path/to/tools --profile ztc6ggyl --version 3 --force-generation
python3 scripts/build.py --tools /path/to/tools --bootloader --force-generation
python3 scripts/build_loader.py --tools /path/to/tools
```

`build/` contains generation/compile logs, generated sources, and a manifest.
The script never flashes a device. `--version N` sets both the compiled application
version and monotonic OTA version. Generated headers are recreated each build.
`build/PROFILE/bridge.bin` is the application at `0x4000`; `ota/` contains the
GBL, Zigbee OTA file, checksums and a model-restricted local Zigpy index.
`--debug-swd` includes a RAM-only diagnostic mailbox for pairing/input injection;
it is absent by default. No radio command accesses that interface.

SLC 5.7 can refuse generation without explaining its SDK validation issues;
`--force-generation` is available for development. A forced generation is recorded
in the manifest and still must compile and pass layout/acceptance checks.
This tool-version mismatch remains a release-tooling issue to resolve.
The SDK also emits an unconfigured-PTI warning while its PTI mode is disabled;
the MCU UART is configured separately on USART0.

Use the compatibility flag in the examples with the pinned SLC version. The
package step rejects a compiled/header version mismatch rather than relabeling
an old binary. Host tests also check vectors, layout bounds and OTA/GBL framing.

The bootloader project is `config/bootloader.slcp`. Its staging slot is
`0x60000..0xB6000`; application vectors start at `0x4000`. The build must reject
application overlap with the slot or an OTA file larger than the slot. NVM is
`0xB6000..0xBE000` with this SDK/part configuration. Verify the linker map for
every target change. No secure-engine firmware or OTP provisioning is required.

The bootloader binary is `build/bootloader-release/bootloader.bin`; the separate
RAM programmer is `build/loader/loader.bin`. The programmer compiles the exact
part's Silicon Labs MSC routines instead of using another family's flash
algorithm. `scripts/swd.py flash --help` lists the required private-backup,
exact-IEEE and image arguments. First conversion needs `--initialize-nvm`.
Use OTA for subsequent application updates and preserve network/configuration storage.

For diagnostic builds, `scripts/debug.py` resolves its mailbox from the exact
ELF and checks the physical target IEEE. It supports pairing, input injection,
route configuration and direct bound-actuator reads. Production builds omit
the mailbox. These commands never substitute for a real MCU/reporting test.
