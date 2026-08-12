# DeutschTelex Phase 7

Phase 7 is a reliability, compatibility, lifecycle, privacy, security, and
release-validation pass. It adds no typing mappings or general product features.

The hardening release is version **0.7.0**. The existing `v0.6.0` tag remains
immutable and points to the Phase 6 packaging milestone; no tag was moved or
overwritten.

## Technical Phase 7A status

Automated Phase 7A verification is complete. Manual Phase 7B acceptance remains
open and is not inferred from these results.

| Area | Result |
|---|---|
| Fresh x64 Release configure/build | PASS |
| Compiler warnings | 0 |
| CTest | 6/6 PASS |
| C++ assertions | 303 passed, 0 failed |
| Extracted portable lifecycle smoke | 20/20 PASS |
| Portable package allowlist | PASS |
| Runtime dependency audit | PASS; Windows DLLs only |
| Production/package path audit | PASS |
| Privacy API/source audit | PASS |
| PE architecture/subsystem | PASS; x64 Windows GUI |
| ASLR/DEP mitigations | PASS |
| Control Flow Guard | Not present in the current MinGW binary |
| Static analysis | NOT TESTED; no analyzer installed |
| ASan/UBSan | NOT TESTED; runtimes unavailable in current MinGW environment |
| Installer build | NOT TESTED; Inno Setup 6 compiler unavailable |
| Interactive application testing | NOT TESTED for 0.7.0 |
| Clean-machine testing | NOT TESTED |

## License review

- License: MIT License.
- Copyright: `Copyright (c) 2026 Hung (daveli1703)`.
- `LICENSE` is included in the portable package and installer configuration.
- Source includes only project files, the C++ standard library, and Windows SDK
  APIs; no external source library is bundled.
- MinGW runtime libraries are statically linked; final PE imports are Windows
  system/API-set DLLs only.
- The supplied PNG/ICO icon was preserved byte-for-byte. No embedded attribution
  or provenance metadata was found, but source inspection cannot establish its
  ownership or redistribution rights. The project owner must confirm the icon's
  origin before public distribution.

## Automated tests

Direct test executable results:

| Suite | Assertions |
|---|---:|
| Release metadata | 5 passed |
| Settings store | 23 passed |
| Transformation engine regression | 159 passed |
| Transformation property/long-stream | 12 passed |
| Win32 components | 104 passed |
| **Total** | **303 passed, 0 failed** |

CTest also runs the application with `--hook-smoke-test`, producing six passing
CTest targets in total.

### Generated property tests

The property suite uses fixed seed `0xd3e075c1e7e10070`. It executes 256 streams
of 4,096 generated events: **1,048,576 reproducible events** containing ASCII
letters, capitals, digits, whitespace, punctuation, Reset operations, and eszett
configuration changes.

The test-only visible-text simulator applies the production engine's semantic
actions. It checks deterministic action/output equality across repeated runs,
valid bounded action shapes, output no longer than processed input, reset/config
semantics, independent engine instances, and no sharp-s output while eszett is
disabled. On failure it reports the synthetic seed, stream index, and length.

### Long-stream tests

Long deterministic streams cover 100,000 repetitions of `ae` and `sz`, plus
50,000 repetitions each of `aee`, `oee`, `uee`, and `szz`. The core remains a
fixed-size, allocation-free semantic object; compile-time checks bound the
engine to at most eight bytes and its snapshot to at most two bytes.

## Bugs found and fixed

### Same-window mouse/caret movement

**Reproduction:** type `a`, click elsewhere inside the same foreground editor,
then type `e`.

**Root cause:** the keyboard layer reset state when the foreground HWND changed,
but a mouse caret move inside the same HWND did not change that identifier. A
pending prefix could therefore survive and apply a Backspace at the new caret.

**Fix:** a minimal `WH_MOUSE_LL` reset-only observer invalidates the transform
FSM and typo checkpoint on button-down and wheel gestures. It stores no mouse
coordinates, target windows, buttons, text, or event history. Mouse movement and
button-up events do no work. It deliberately does not clear suppressed keyboard
key-ups, preventing unmatched release events.

**Regression tests:** message classification is tested for left/right/middle/X
button-down, vertical/horizontal wheel, movement, and button-up cases. The hook
is also installed and removed by lifecycle smoke tests.

Interactive confirmation in Notepad is still required.

### Release directory lock

The first clean release attempt correctly failed because the old Phase 6
`build-release\DeutschTelex.exe` was still running. After its exact executable
path and PID were verified, that old instance was stopped and the clean build
was rerun successfully. No stale binary was reused.

## Win32 reliability review

### Keyboard and mouse hooks

- Hooks have one active-instance pointer each and reject duplicate installation.
- Installation failure paths release previously acquired application resources.
- Destructors and explicit shutdown both uninstall safely.
- Callbacks perform bounded state/classification work and call the next hook.
- There is no file, registry, network, logging, sleep, UI, or lock operation in
  either hook callback.
- Foreground process identification is performed only on foreground HWND change
  and only when the optional VS Code exclusion is active. The path is discarded
  immediately after extracting the executable filename.

### Input injection

- A complete replacement is prebuilt as one bounded `INPUT` array.
- Every generated event carries the DeutschTelex `dwExtraInfo` marker.
- Own generated events pass through without re-entering the engine.
- Foreign injected events pass through and reset uncertain transform state.
- Zero or partial `SendInput` insertion is failure; no blind retry occurs.
- Injection failure resets the FSM/checkpoint and passes the physical trigger.
- Each replacement contains exactly one Backspace pair; there is no repeated
  deletion retry that could remove unrelated text.

### Suppressed physical key releases

Suppression is a fixed 256-key bitset exposed through a narrow state object.
Tests cover a normal consumed trigger, consume-once behavior, auto-repeat clearing,
lifecycle clearing, and out-of-range virtual keys. Mouse context resets leave
this bookkeeping intact because a corresponding physical key-up may still be
required.

### Hotkey, instance, and lifecycle

- `Ctrl+Alt+G` is registered with `MOD_NOREPEAT`; failure leaves tray controls
  usable and produces a non-blocking notification.
- Production and portable copies share one named per-session mutex, intentionally
  allowing only one active instance.
- Shutdown order removes the mouse hook, disables/uninstalls the keyboard hook,
  unregisters the hotkey, removes the tray icon, closes Settings and coordinator
  windows, then closes the mutex.
- Twenty extracted portable smoke cycles passed with no hook-install, teardown,
  mutex, or process-exit failure.
- An unexpected process termination cannot leave a persistent keyboard filter:
  both integrations are user-mode low-level hooks owned by the process, with no
  driver, service, or system keyboard-layout modification.

### Resource lifetime

Reviewed resources and cleanup:

- `HHOOK`: explicit uninstall plus destructor fallback for keyboard and mouse.
- `HWND`: modeless Settings and coordinator windows destroyed during shutdown.
- `HICON`: destroyed after tray removal or failed tray creation.
- `HMENU`: destroyed after each popup invocation.
- process `HANDLE`: closed immediately after foreground executable query.
- registry `HKEY`: closed on every successful open/create path.
- named mutex `HANDLE`: closed on duplicate detection, failure, and shutdown.
- global hotkey: unregistered only when registration succeeded.
- tray icon: removed with `NIM_DELETE` before its icon handle is destroyed.

No confirmed native resource leak was found.

## Settings and startup review

Automated settings tests cover missing directory/file, empty file, missing keys,
unknown sections/keys, invalid Booleans, partially valid input, round trips, and
a stale `.tmp` file. Saving truncates and flushes the temporary file, then uses
`MoveFileExW` with replacement and write-through flags. Failed saves clean the
temporary file where possible.

Startup remains a quoted current executable path in:

```text
HKCU\Software\Microsoft\Windows\CurrentVersion\Run\DeutschTelex
```

It is per-user and requires no elevation. Saving coordinates startup and INI
changes with rollback if persistence fails. Moving a portable executable after
enabling startup remains a documented limitation.

## Security and privacy audit

Source inspection found no socket/HTTP client APIs, clipboard APIs, target-text
read APIs, debug text dumps, typed-event logs, telemetry, analytics, update
checks, crash uploads, accounts, cloud synchronization, or application-history
storage.

Allowed transient state remains:

- constant-size transform FSM and configuration;
- single-use semantic typo checkpoint;
- modifier and suppressed-key state;
- current foreground HWND and coarse current app identity;
- ordinary user preferences;
- reset-only mouse callback state with no event data retained.

The hook does not persist or transmit typed characters. The Settings store is the
only production source file that writes a file, and it writes known Boolean
preferences rather than input data.

Windows UIPI may prevent a normal DeutschTelex process from injecting into a
higher-integrity target. DeutschTelex does not elevate automatically, request
administrator rights, install a service, or install a driver.

## Binary and dependency audit

- Compiler: GCC 15.2.0, MSYS2 UCRT64.
- Standard: C++20.
- Architecture: PE x86-64.
- Subsystem: Windows GUI; no console window expected.
- File version: 0.7.0.0.
- Product version: 0.7.0.
- `HIGH_ENTROPY_VA`: present.
- `DYNAMIC_BASE`/ASLR: present.
- `NX_COMPAT`/DEP: present.
- Control Flow Guard: not present in the current MinGW output. No unverified
  linker change was made solely to claim it.
- Digital signature: NotSigned; unsigned development release.
- Required non-system DLLs: none.
- No dependency on GCC executables, CMake, Codex, VS Code, OneDrive, or the
  source/build tree was detected in the packaged executable.

Static analysis was not run because cppcheck/clang-tidy is not installed.
ASan/UBSan is unavailable in the current MinGW environment; the compiler reports
no usable sanitizer libraries. No toolchain was installed solely for Phase 7.

## Release artifacts

- Portable folder: `dist\DeutschTelex-0.7.0-win64`
- Portable ZIP: `dist\DeutschTelex-0.7.0-win64-portable.zip`
- ZIP size: 984,719 bytes
- SHA-256:
  `163196b467aacd1a4593e730dee3dfc06a8d2f982b75a00bdda88a2194d6acc0`
- Installer: not built; Inno Setup 6 compiler unavailable.

The portable archive is enforced against an explicit allowlist and contains one
root directory with the executable, README, MIT LICENSE, changelog, 0.7.0 release
notes, and runtime dependency report. It does not contain source, tests, build
files, the excluded technical document, or development paths.

Installer configuration preserves the Phase 6 per-user design and now displays
and installs the MIT License. It must be compiled and manually tested before an
installer is offered.

## Compatibility and manual Phase 7B checklist

Results belong in `docs/compatibility.md`. Use only synthetic, non-sensitive test
text. Keep every result NOT TESTED until actually exercised.

### Portable and clean machine

1. Verify the ZIP SHA-256.
2. Extract to a new folder on another Windows 10/11 x64 machine.
3. Record SmartScreen and Defender behavior without disabling security.
4. Launch and confirm no console, correct tray icon, Settings left-click, and one
   active instance.
5. Test `Maedchen`, `schoen`, `fuer`, `aee`, `szz`, escape behavior, and typo
   recovery.
6. Type `a`, click elsewhere in the same Notepad document, then type `e`; verify
   no unrelated transformation. Repeat `s`, click, `z`.
7. Test `Ctrl+Alt+G`, Settings persistence, startup, VS Code exclusion, and Exit.
8. Exercise installed applications listed in `docs/compatibility.md` without
   installing extra software solely for the test.

### Keyboard stress and editing

1. Hold `a`, `e`, `o`, `u`, `s`, `z`, Backspace, and Shift separately; after
   release, verify no stuck/missing key state.
2. Type the fast streams specified in the Phase 7 request repeatedly and check
   for drops, duplication, missed conversions, and stale candidates.
3. Test Shift, Caps Lock, and Shift+Caps Lock without broadening existing rules.
4. Verify Ctrl+C/V/X/A/Z/Y/F/S, Alt+Tab/F4, and Win+D/E/R/L.
5. Verify Backspace, Delete, Enter, Tab, Escape, arrows, Home, End, Page Up, and
   Page Down reset pending candidates.

### Lifecycle and UI

1. Toggle repeatedly while idle, after prefixes/conversions/corrections, after
   Alt+Tab, in VS Code, and while Settings is open.
2. Stress one Settings window with Save, Cancel, Defaults, and every preference.
3. Enable/disable/re-enable startup and test sign-out/sign-in where practical.
4. Try repeated launches and installed/portable copies; expect one instance.
5. Perform at least 20 interactive launch/type/settings/exit cycles.
6. Test sleep/resume and lock/unlock; secure-desktop operation is not expected.
7. Check Settings at 100%, 125%, and 150% scaling and across monitors if available.
8. Test a normal process against an elevated disposable target and record UIPI
   behavior without elevating DeutschTelex.

### Installer, if later compiled

Verify per-user installation without unnecessary elevation, expected Local
AppData path, Start Menu shortcut/icon, application behavior, upgrade without
duplicates, settings preservation, and uninstall removal of application files,
shortcuts, and only the matching startup entry. Do not publish the installer
until this checklist passes.

## Manual work remaining and release blockers

- All interactive application compatibility is NOT TESTED for 0.7.0.
- Clean Windows 10 and Windows 11 machine acceptance is NOT TESTED.
- SmartScreen and Defender behavior is NOT TESTED.
- Sleep/resume, lock/unlock, DPI, elevated-target, held-key, and fast-typing
  checks are NOT TESTED.
- The installer is not compiled or manually tested.
- The supplied icon's redistribution provenance is unresolved.
- The executable and future installer are unsigned.
- GitHub Release publication has not been authorized and was not performed.

Phase 7A automation passing does not make 0.7.0 ready for public release until
the relevant Phase 7B evidence and icon rights are supplied.

## Git state policy

Phase 7 implementation did not silently rewrite a remote or publish a GitHub
Release. The owner separately authorized committing the reviewed source,
creating `v0.7.0`, and pushing both to the expected
`https://github.com/daveli1703/DeutschTelex.git`. GitHub Release publication
still requires separate authorization.

`docs/how-deutschtelex-was-built.md` remains untracked and excluded: it was not
modified, staged, committed, packaged, or referenced from README. Any future
staging must name files explicitly and verify `git diff --cached --name-only`;
do not use `git add .` while that excluded file remains present.
