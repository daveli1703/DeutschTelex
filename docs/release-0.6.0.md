# DeutschTelex 0.6.0

DeutschTelex 0.6.0 is a pre-1.0 Windows development release focused on branding,
portable distribution, and installer preparation. Input behavior is unchanged
from the accepted 0.5.0 baseline.

## Highlights

- Native Windows x64 Release build
- Supplied DeutschTelex icon embedded in the executable and used by the tray
- Windows Explorer version and product metadata
- Standalone portable ZIP
- Reproducible release-build script
- Per-user Inno Setup installer configuration

## Installation

If `DeutschTelex-0.6.0-win64-setup.exe` is provided, run it to install under
`%LOCALAPPDATA%\Programs\DeutschTelex`. The installer requires no administrator
privileges and creates a Start Menu shortcut. It does not silently enable Start
with Windows.

Uninstall removes installed application files and shortcuts. It preserves
`%LOCALAPPDATA%\DeutschTelex\settings.ini`. It removes the DeutschTelex Run entry
only when that entry points exactly to the installed executable.

## Portable version

Extract `DeutschTelex-0.6.0-win64-portable.zip` into a new folder and run
`DeutschTelex.exe`. Preferences continue to use the normal Local AppData settings
file. Moving the executable after enabling Start with Windows leaves the old path
registered until the setting is updated.

## Default mappings

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

`ss` remains unchanged.

## Settings

- Start with Windows
- ON/OFF notifications
- Optional `sz → ß`
- Optional Visual Studio Code exclusion

## Privacy

All keyboard processing is local. DeutschTelex has no telemetry, analytics,
networking, update checks, typed-text logging, clipboard inspection, or
target-document inspection.

## Known limitations

- This build is unsigned. Windows SmartScreen may show an unknown-publisher
  warning. Do not disable Windows security globally.
- Clean-machine compatibility still requires testing on another Windows 10/11
  x64 computer.
- Elevated applications, secure desktops, some games, unusual terminals, remote
  desktop environments, and applications rejecting injected Unicode may not work.
- The VS Code exclusion recognizes `Code.exe` only.
- No automatic update mechanism is included.

## License status

No repository license has been selected. The repository owner must choose and add
a LICENSE before public distribution.
