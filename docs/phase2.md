# DeutschTelex Phase 2 proof of concept

Phase 2 is a console-based keyboard-integration test. It has no tray icon,
settings, installer, networking, telemetry, clipboard access, or text logging.

## Launch and stop

From PowerShell:

```powershell
& '.\build-phase2\DeutschTelex.exe'
```

Keep the console process running while testing. Focus the console and press
Ctrl+C to remove the hook and exit cleanly. A named per-session mutex prevents a
second copy from installing another hook.

## Notepad acceptance test

1. Start DeutschTelex and confirm that it prints `Keyboard hook active.`
2. Open a new Notepad document.
3. Type each input on a separate line and compare the visible result:

| Input | Expected |
|---|---|
| `Maedchen` | `Mädchen` |
| `schoen` | `schön` |
| `fuer` | `für` |
| `aee` | `ae` |
| `oee` | `oe` |
| `uee` | `ue` |
| `sz` | `ß` |
| `szz` | `sz` |
| `dass` | `dass` |
| `wissen` | `wissen` |

4. Verify that Ctrl+C, Ctrl+V, Ctrl+Z, and Alt+Tab retain their normal meanings.
5. After typing a partial candidate, exercise Backspace, Delete, every arrow
   key, Enter, and Tab. The editing/navigation action should occur normally and
   the next character must not complete the old candidate.
6. Type `a` in Notepad, switch to another application, and type `e`. The second
   application must receive a literal `e`; it must not delete or modify text.
7. Test the single-character typo correction. Each input sequence below should
   leave the expected final text:

| Keys | Expected |
|---|---|
| `a`, `w`, Backspace, `e` | `ä` |
| `o`, `w`, Backspace, `e` | `ö` |
| `u`, `w`, Backspace, `e` | `ü` |
| `s`, `w`, Backspace, `z` | `ß` |
| `A`, `w`, Backspace, `e` | `Ä` |

8. Confirm the checkpoint is deliberately single-use: `a`, `w`, `x`,
   Backspace, `e` must not turn the earlier `a` into `ä`. Also verify that
   `ae`, `w`, Backspace leaves `ä`; typing `e` afterward must produce `äe`,
   not an escape back to `ae`.

## Stress checks

- Type the acceptance strings as quickly as possible several times.
- Hold each of `a`, `e`, `s`, and `z`, then release it; afterward confirm normal
  typing still works. Auto-repeat is processed as repeated input. A suppressed
  trigger key-up is tracked only until its matching release or until a later
  passed repeat requires that key-up.
- Test lowercase letters, Shift-modified letters, and Caps Lock. Supported
  capital forms remain `Ae`, `Oe`, and `Ue`; `AE`, `OE`, and `UE` remain literal.
- Type `ae ae ae ae ae` and expect `ä ä ä ä ä`.
- Type `Das Maedchen findet Koeln schoen.` and expect
  `Das Mädchen findet Köln schön.`
- Mix `aee`, `oee`, `uee`, and `szz` into ordinary text.
- Repeated independent corrections such as `a`, `w`, Backspace, `x`,
  Backspace, `e` are supported because each new typo is a fresh one-character
  checkpoint. They are not an undo history: any second printable character
  before a Backspace invalidates the older checkpoint.
- Type a prefix in one application, switch windows, and type its trigger in the
  other application.
- While a key is held, try a transformation and then confirm key-up/release does
  not leave subsequent typing stuck.

## Known limitations

This `WH_KEYBOARD_LL` plus `SendInput` proof of concept may not work reliably in:

- elevated applications when DeutschTelex is not elevated;
- secure Windows desktops, including sign-in and UAC consent screens;
- games or applications using unusual/raw input handling;
- some terminals and remote-desktop configurations;
- applications that reject synthetic Unicode input.

Only a foreground-window change is detected. Moving the caret with the mouse
inside the same foreground window is not detected because Phase 2 deliberately
has no mouse hook. The decoder intentionally targets ordinary Latin QWERTY
virtual keys and is not a general keyboard-layout translator or IME.

The program does not elevate itself. It does not inspect application text, read
the clipboard, save keyboard history, log typed characters, or transmit data.
