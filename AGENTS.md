# Project scope

Keep the bridge generic: no room names, personal host addresses, target relay
IEEE addresses, network keys or stock dumps in source, tests, or documentation.
Board/product differences belong in explicit profiles. The initial supported
radio is EFR32MG21A020F768IM32; do not advertise untested binary compatibility.

Preserve absolute On/Off behavior, one configurable source endpoint, standard
destination bindings, persistent validated configuration, and no output before
known input. Home Assistant quirks expose configuration and telemetry only.

Run `make test` for parser/control/package changes. Build both profiles when
changing shared firmware. Recreate generated SLC headers on every build;
verify ApplicationProperties and OTA versions agree before packaging.

Before hardware writes, identify the exact chip/IEEE and preserve two matching
private stock dumps. Use targeted pages and readback, never routine mass erase,
OTP provisioning, secure-engine updates or identity replacement. Keep hardware
acceptance evidence separate from compilation and protocol acknowledgements.
Do not record simulated input tests as observed physical occupancy transitions.
