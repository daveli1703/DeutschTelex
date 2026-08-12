# DeutschTelex 0.7.0

DeutschTelex 0.7.0 is a Windows reliability and release-hardening update. It
does not add typing mappings or product features.

## Highlights

- MIT-licensed project source and release documentation
- License included in the portable package and installer configuration
- Deterministic generated property tests and long-stream tests
- Same-window mouse/caret safety reset without storing mouse coordinates
- Regression coverage for partial input injection and suppressed key releases
- Compatibility, privacy, dependency, lifecycle, and binary-hardening audits

## Portable version

Download `DeutschTelex-0.7.0-win64-portable.zip`, verify its SHA-256 against
`SHA256SUMS.txt`, extract the complete archive, and run `DeutschTelex.exe`.

Preferences are stored in `%LOCALAPPDATA%\DeutschTelex\settings.ini`. Installed
and portable copies intentionally share those preferences and the production
single-instance identity.

## Typing rules

| Input | Output |
|---|---|
| `ae` | `ä` |
| `aee` | `ae` |
| `oe` | `ö` |
| `oee` | `oe` |
| `ue` | `ü` |
| `uee` | `ue` |
| `Ae` | `Ä` |
| `Oe` | `Ö` |
| `Ue` | `Ü` |
| `sz` | `ß` |
| `szz` | `sz` |

`ss` and unsupported mixed/all-uppercase forms remain unchanged.

## Privacy

All input processing is local. DeutschTelex has no telemetry, analytics,
networking, update checks, typed-text logging, keyboard history, clipboard
inspection, or target-document inspection. The mouse safety hook uses button or
wheel messages only as a reset signal and retains no coordinates or history.

## Compatibility status

Automated tests validate the core engine and Win32 components, but they do not
prove interactive compatibility with every application. See
`docs/compatibility.md` for evidence-based PASS, PARTIAL, FAIL, and NOT TESTED
statuses. Clean-machine and interactive 0.7.0 acceptance remain required before
publishing a release.

## Known limitations

- This is an unsigned development release, so SmartScreen may warn about an
  unknown publisher.
- Windows integrity boundaries can prevent a normal process from injecting into
  an elevated application.
- Secure desktops, raw-input applications, some games, unusual terminals, and
  some remote sessions may not work.
- The optional application exclusion recognizes Visual Studio Code only.
- Moving a portable executable after enabling startup leaves its old path in the
  current-user startup entry until the setting is updated.
- The installer configuration requires Inno Setup 6 and must be compiled and
  manually accepted before an installer is distributed.

## License and supplied icon

DeutschTelex source is released under the MIT License. The supplied icon is
preserved unchanged, but its provenance is not recorded in the repository; its
redistribution status must be confirmed by the project owner before public
distribution.
