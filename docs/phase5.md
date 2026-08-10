# DeutschTelex Phase 5

Phase 5 adds one optional application exclusion: Visual Studio Code. It does not
add a generic exclusion list, executable browser, or per-application profiles.

## Setting and persistence

The default is OFF. The existing INI gains one section:

```ini
[Applications]
DisableInVSCode=false
```

The file remains `%LOCALAPPDATA%\DeutschTelex\settings.ini`. Missing or invalid
`DisableInVSCode` values fall back to `false`, and unknown keys remain ignored.
Defaults changes this checkbox to OFF but still requires Save before anything is
persisted or applied.

## Foreground detection and caching

The keyboard hook remains installed. On each physical keyboard event it compares
the current foreground HWND with one cached HWND. Only when that handle changes,
and only while the exclusion preference is enabled, DeutschTelex:

1. obtains the foreground window's process ID;
2. opens that process with `PROCESS_QUERY_LIMITED_INFORMATION`;
3. obtains its executable path through `QueryFullProcessImageNameW`;
4. extracts the final executable filename;
5. compares it exactly and ASCII case-insensitively with `Code.exe`;
6. discards the path and caches only `VisualStudioCode`, `Other`, or `Unknown`.

No application history is kept. `MyCode.exe`, `CodeHelper.exe`, and
`code.exe.backup` do not match. Any normal VS Code child UI hosted by `Code.exe`,
including its integrated terminal, receives the same exclusion decision.

Every foreground-context change resets the transformation FSM and typo
checkpoint. Changing the preference also resets those states immediately. While
excluded, printable input is passed through and never fed into the transformation
engine or typo checkpoint.

If process identification fails, stale identity is discarded and the cache is
`Unknown`. When the exclusion preference is enabled, `Unknown` conservatively
bypasses transformation for that foreground HWND. No notification is shown. The
identity is resolved again after the foreground HWND changes or the setting is
reapplied.

Global OFF always bypasses all applications. Global ON plus exclusion OFF
transforms everywhere. Global ON plus exclusion ON bypasses only positively
identified VS Code, with the conservative unknown fallback described above.
Ctrl+Alt+G remains a registered system hotkey and is independent of exclusion.

## Manual Windows acceptance checklist

1. Exit an older DeutschTelex build, launch the Phase 5 executable, and confirm
   there is no console, the tray icon appears, and the application starts ON.
2. Open Settings. Confirm the APPLICATIONS section contains `Disable DeutschTelex
   in Visual Studio Code` and that it defaults to unchecked.
3. With the exclusion unchecked, open VS Code and type `ae`, `oe`, `ue`, and `sz`.
   Confirm they produce `ä`, `ö`, `ü`, and `ß` when the eszett rule is enabled.
4. Enable the VS Code exclusion and Save. In VS Code, confirm the same sequences
   remain `ae`, `oe`, `ue`, and `sz`.
5. In Notepad, confirm those sequences still produce `ä`, `ö`, `ü`, and `ß`.
6. In Notepad type `a`, switch to VS Code, then type `e`. Confirm VS Code receives
   only `e`, not `ä`. In VS Code type `a`, switch to Notepad, then type `e` and
   confirm Notepad receives only `e`.
7. Alternate between Notepad and VS Code several times. Confirm Notepad transforms
   and VS Code does not.
8. With VS Code foreground, press Ctrl+Alt+G. Confirm global ON/OFF toggles and no
   normal text is produced.
9. In Notepad type `a`, a wrong printable character, Backspace, `e`; confirm `ä`.
   In excluded VS Code type `a`, `w`, Backspace, `e`; confirm literal `ae`.
10. Disable the exclusion, Save, and return to VS Code without restarting. Confirm
    `ae` becomes `ä` immediately.
11. Enable the exclusion, Save, exit DeutschTelex, restart it, and confirm the
    checkbox remains enabled and VS Code remains excluded.
12. Click Defaults and confirm the VS Code checkbox becomes OFF. Cancel and confirm
    the saved ON setting remains. Repeat Defaults and Save; restart and confirm OFF
    persisted.
13. Confirm the existing tray menu, left-click Settings behavior, About, Exit,
    Start with Windows, notification preference, and eszett preference still work.

Automated tests do not inspect live VS Code windows. Live foreground-process
detection and application switching require the manual checks above.

## Privacy

Phase 5 reads only the current foreground HWND, its process ID, and the executable
path needed transiently to extract the process filename. It does not inspect or
persist editor text, document filenames, tabs, workspaces, clipboard data, VS Code
settings, or foreground history. No telemetry, networking, or typed-text logging
was added.
