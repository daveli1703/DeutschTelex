# DeutschTelex 0.8.0 compatibility

Statuses are evidence-based:

- **PASS**: manually exercised successfully with this release.
- **PARTIAL**: usable with a documented limitation.
- **FAIL**: a reproducible incompatibility exists.
- **NOT TESTED**: no manual evidence exists for this release.

Automated component tests and a hook-lifecycle smoke test are not treated as
proof of interactive application compatibility. Phase 5 and earlier behavior
was manually accepted on the development machine. Version 0.8.0 leaves the
typing engine and hook behavior unchanged, but the refreshed Settings interface
and release artifacts still require beta feedback on other machines.

| Application | Version | Basic typing | Escape | Typo recovery | Hotkey | Notes |
|---|---|---:|---:|---:|---:|---|
| Notepad | Not recorded | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | Test 0.8.0, including clicking elsewhere after a partial prefix. |
| Windows Search | Not recorded | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | Do not test sensitive searches. |
| Run dialog | Not recorded | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | No private paths should be recorded in results. |
| File Explorer rename | Not recorded | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | Test a disposable filename. |
| Chrome | Not recorded | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | Test address bar and ordinary fields separately. |
| Edge | Not recorded | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | Test address bar and ordinary fields separately. |
| Firefox | Not recorded | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | Test only if already installed. |
| Word | Not recorded | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | Test only if available. |
| Excel cell editor | Not recorded | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | Test only if available. |
| Outlook editor | Not recorded | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | Use non-sensitive test text. |
| Discord | Not recorded | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | Test only if available. |
| Slack | Not recorded | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | Test only if available. |
| Telegram Desktop | Not recorded | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | Test only if available. |
| VS Code, exclusion OFF | Not recorded | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | Check editor, terminal, search, and command palette. |
| VS Code, exclusion ON | Not recorded | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | Input should remain literal throughout `Code.exe`. |
| Windows Terminal | Not recorded | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | Test only if available. |
| PowerShell | Not recorded | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | Use disposable commands/text. |
| Command Prompt | Not recorded | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | Use disposable commands/text. |

## Operating systems

| Operating system | Status | Evidence |
|---|---|---|
| Windows 11 x64 | NOT TESTED | Automated development build and hook smoke test are not interactive acceptance. |
| Windows 10 x64 | NOT TESTED | No Windows 10 machine was available during automated Phase 7 work. |

## Known platform boundaries

- Elevated targets may reject input from non-elevated DeutschTelex due to UIPI.
- DeutschTelex is not expected to operate on sign-in, UAC, or other secure desktops.
- Raw-input software, games, remote sessions, or applications rejecting injected
  Unicode may be PARTIAL or FAIL when tested.
- Installed and portable copies share settings and a single-instance identity;
  only one is expected to run.
