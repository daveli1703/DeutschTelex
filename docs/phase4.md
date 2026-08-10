# DeutschTelex Phase 4

Phase 4 adds native settings, per-user preference persistence, and optional
Start with Windows registration. It does not add application exclusions,
custom mappings, or configurable hotkeys.

## Architecture

- `config::AppSettings` is the three-Boolean preference value type.
- `config::SettingsStore` loads and atomically replaces a small INI file.
- `win32::StartupRegistration` manages the current user's Run entry.
- `app::SettingsWindow` owns the modeless native controls and Save/Cancel/Defaults
  behavior.
- `app::TrayApp` coordinates those components and applies successfully saved
  settings.
- `core::TransformConfig` carries only `enable_eszett`; the core remains free of
  Windows types.

The Settings window is modeless so the tray message loop and keyboard hook stay
responsive. It is owned by the hidden coordinator window and uses the tool-window
style to avoid a separate taskbar button. Selecting Settings again focuses the
existing instance.

A normal left-click on the custom DeutschTelex tray icon opens Settings directly.
Right-click retains the ON/OFF, Settings, About, and Exit menu.

## Preferences

Defaults:

```ini
[General]
StartWithWindows=false
ShowNotifications=true

[Input]
EnableEszett=true
```

The file is stored at:

```text
%LOCALAPPDATA%\DeutschTelex\settings.ini
```

Missing files, directories, sections, and keys use defaults. Invalid Boolean
values use that field's default, and unknown settings are ignored. Saving writes
`settings.ini.tmp`, flushes it, then atomically replaces `settings.ini`. Only
these preferences are persisted; ON/OFF state and all typing state remain
ephemeral.

## Start with Windows consistency

Start with Windows uses this per-user, non-admin registry value:

```text
HKCU\Software\Microsoft\Windows\CurrentVersion\Run\DeutschTelex
```

The value is the dynamically discovered executable path enclosed in quotes.
During Save, a requested startup change is made first. The INI is then replaced.
If INI saving fails, the registry change is rolled back; the in-memory settings
are applied only after both operations succeed. If rollback itself fails, the
Settings window stays open and reports the inconsistency explicitly.

At startup, the actual registry presence is authoritative for the checkbox. This
keeps the UI honest if the Run entry was changed outside DeutschTelex.

## Manual Windows acceptance checklist

1. Exit any older DeutschTelex build and launch the Phase 4 executable. Confirm
   no console appears, the tray icon appears, and DeutschTelex starts ON.
2. Verify `Maedchen`, `schoen`, `fuer`, `aee`, and `sz` retain their Phase 3
   results. Verify Ctrl+Alt+G and the tray ON/OFF command.
3. Open Tray > Settings. Select Settings again and confirm it focuses the same
   window instead of opening a duplicate.
4. Disable Show ON/OFF notifications and Save. Toggle with Ctrl+Alt+G: the state
   and tooltip must change without a normal balloon. Re-enable and Save; balloons
   should return.
5. Exit and restart. Confirm the notification preference persisted.
6. Disable Enable `sz → ß` and Save. Confirm `sz` and `szz` remain literal while
   `ae`, `oe`, and `ue` still become `ä`, `ö`, and `ü`. Re-enable and confirm
   `sz` becomes `ß` again.
7. Type `s`, open Settings, disable `sz → ß`, Save, return to the target, and type
   `z`. Confirm the visible result is `sz`, not `ß`.
8. Change controls, press Defaults, and confirm OFF/ON/ON. Press Cancel and reopen;
   confirm saved settings were unchanged. Repeat Defaults and press Save; reopen
   and confirm defaults persisted.
9. Enable Start with Windows and Save. Confirm the `DeutschTelex` value exists
   under the HKCU Run key and contains the quoted path to this executable. Sign
   out/restart if practical; confirm there is no administrator prompt, the tray
   icon appears, and the app starts ON. Disable the option and confirm the value
   is removed.
10. With DeutschTelex exited, set `EnableEszett=potato` in the INI and restart.
    Confirm the app does not crash and the checkbox/rule safely defaults to ON.
11. Verify typo restoration and Ctrl+C, Ctrl+V, Ctrl+Z, Ctrl+A, and Alt+Tab.
12. Choose Tray > Exit. Confirm the hook, hotkey, and icon disappear. Restart and
    confirm preferences remain saved while runtime ON/OFF state starts ON.

Automated tests never modify the real Local AppData settings file or the real
startup registry entry. Those integration points require the manual checks above.
