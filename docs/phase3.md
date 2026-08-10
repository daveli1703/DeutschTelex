# DeutschTelex Phase 3

Phase 3 adds a native Win32 system-tray application around the existing keyboard
input layer. The app owns a hidden coordinator window, the tray icon/menu, the
`Ctrl+Alt+G` hotkey, and clean shutdown. The core transformation engine remains
independent of this layer.

## Enable state

DeutschTelex starts **ON**. The keyboard hook remains installed while it is OFF,
but immediately passes input through without inspecting it for transformations.
Each ON/OFF transition resets the hook's transformation engine, single-typo
checkpoint, modifier state as appropriate, and suppressed-key bookkeeping. A
partial candidate therefore cannot survive a toggle.

The tray tooltip is authoritative: `DeutschTelex — ON` or `DeutschTelex — OFF`.
The MVP uses the standard Windows application icon for both states; the tooltip
provides the state distinction.

Right-click the tray icon for the ON/OFF command, About, and Exit. Left-click
intentionally does nothing, which avoids accidental input-mode changes.

If `Ctrl+Alt+G` cannot be registered, DeutschTelex remains usable through the
tray menu and displays a non-blocking tray notification explaining that the
hotkey is unavailable.

## Manual Windows checklist

1. Launch `DeutschTelex.exe`. Confirm no console window appears, a tray icon
   appears, and its tooltip is `DeutschTelex — ON`.
2. In Notepad, verify `Maedchen`, `schoen`, `fuer`, `aee`, and `szz` produce
   `Mädchen`, `schön`, `für`, `ae`, and `sz` respectively.
3. Verify typo correction: type `a`, an arbitrary wrong printable character,
   Backspace, then `e`; the final result should be `ä`.
4. Press Ctrl+Alt+G. Confirm the tooltip/notification reports OFF, and `ae`
   remains `ae`. Press it again and confirm `ae` becomes `ä`.
5. Type `a`, toggle OFF, toggle ON, then type `e`. Confirm the result is `e`.
6. Repeat the ON/OFF checks through the tray menu.
7. Verify Ctrl+C, Ctrl+V, Ctrl+Z, Ctrl+A, and Alt+Tab behave normally.
8. Select tray menu Exit. Confirm the icon disappears and typing `ae` remains
   `ae` immediately afterward.

No typed text is logged, saved, transmitted, inspected in target applications,
or retained beyond the existing minimal input FSM state.
