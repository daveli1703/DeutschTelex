# DeutschTelex 0.8.0 public beta

DeutschTelex 0.8.0 modernizes the native Settings interface while preserving
the deterministic typing behavior introduced in earlier releases.

## Highlights

- Cleaner native Win32 Settings window with a card-based layout
- Segoe UI typography, clearer spacing, and improved keyboard accessibility
- Per-monitor DPI scaling with compatibility fallbacks for Windows 10
- Current Windows Common Controls visual styles
- Existing local-only privacy model and typing rules remain unchanged

## Downloads

- Installer: `DeutschTelex-0.8.0-win64-setup.exe`
- Portable: `DeutschTelex-0.8.0-win64-portable.zip`
- Integrity hashes: `SHA256SUMS.txt`

Verify the selected download against `SHA256SUMS.txt` before running it. The
installer is per-user and does not require administrator privileges. The
portable archive can be extracted anywhere writable and started by running
`DeutschTelex.exe`.

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

`ss` and unsupported mixed or all-uppercase forms remain unchanged.

## Privacy

All input processing is local. DeutschTelex has no telemetry, analytics,
networking, update checks, typed-text logging, keyboard history, clipboard
inspection, or target-document inspection. It retains only the minimum
temporary semantic state required for deterministic conversion and safe typo
recovery.

## Beta status

Automated tests cover the transformation engine, settings persistence, Win32
components, release metadata, and hook lifecycle. They do not prove interactive
compatibility with every Windows application. This release is published as a
beta so testers can report application-specific behavior. Evidence-based
statuses are tracked in `docs/compatibility.md`.

Useful reports include the Windows version, target application, exact keys
typed, expected output, actual output, and whether DeutschTelex was enabled.
Never include private text in a report.

## Known limitations

- The binaries are unsigned, so Windows SmartScreen may warn about an unknown
  publisher.
- Elevated applications may reject input from a non-elevated DeutschTelex
  process.
- Secure desktops, raw-input applications, some games, unusual terminals, and
  some remote sessions may not work.
- The optional application exclusion recognizes Visual Studio Code only.
- A Start with Windows entry for a portable copy does not follow the executable
  if it is later moved.
