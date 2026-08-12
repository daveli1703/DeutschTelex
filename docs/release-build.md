# Release build instructions

The authoritative version is the CMake `project()` version. CMake generates the
C++ version header, Windows VERSIONINFO resource, and installer script from it.

## Automated portable build

From a PowerShell prompt with CMake, a C++20 compiler, and a build tool available:

```powershell
.\scripts\build-release.ps1
```

Optional tool overrides are available for environments where CMake, Ninja, the
compiler, or `objdump` are not on `PATH`:

```powershell
.\scripts\build-release.ps1 `
  -CMakePath C:\path\to\cmake.exe `
  -Generator Ninja `
  -CompilerPath C:\path\to\g++.exe `
  -BuildToolPath C:\path\to\ninja.exe `
  -ObjdumpPath C:\path\to\objdump.exe
```

The script:

1. creates a clean `build-release` directory;
2. configures and compiles Release with tests enabled;
3. refuses to package if CTest fails;
4. verifies version metadata and Windows x64 GUI subsystem;
5. inspects imported runtime DLLs;
6. stages only user-facing files;
7. creates `dist\DeutschTelex-0.7.0-win64-portable.zip`;
8. builds the installer when Inno Setup 6 is available.

## Installer

CMake configures `generated\DeutschTelex.iss` in the release build directory.
Install Inno Setup 6 separately, then rerun the release script to produce:

```text
dist\DeutschTelex-0.7.0-win64-setup.exe
```

The installer is per-user and targets
`%LOCALAPPDATA%\Programs\DeutschTelex`. It creates a Start Menu shortcut, offers
an unchecked post-install launch option, preserves user settings on uninstall,
and removes only a matching installed-path startup entry.

## Signing

No code-signing certificate is configured. The produced executable and installer
are unsigned development artifacts. Authentic signing can be added later; never
store private signing keys in the repository.

The future signing order is: build, automated tests, sign the executable,
package, sign the installer, generate SHA-256 checksums, then publish the GitHub
Release.
