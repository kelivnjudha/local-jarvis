# Windows Build

Local Jarvis uses CMake presets and a vcpkg manifest for reproducible core dependencies. The Qt desktop app is optional: core-only builds do not require Qt.

Generated build output is written under:

```text
out/build/<preset-name>
```

The `out/` directory is ignored by Git and should not be committed.

## Developer Scripts

Use the scripts from the repository root in PowerShell.

Bootstrap a developer machine and run the Debug core build/test path:

```powershell
.\scripts\bootstrap-windows.ps1
```

The bootstrap script checks Git, CMake, MSVC, vcpkg, and Qt discovery. If `VCPKG_ROOT` is missing, it suggests `C:\dev\vcpkg`. If vcpkg is not present there, it asks before cloning `https://github.com/microsoft/vcpkg.git`. It always sets `VCPKG_DISABLE_METRICS=1` for the current process.

Run repeatable verification after the machine is already set up:

```powershell
.\scripts\verify-windows.ps1
```

The verify script runs both core presets and their tests. If Qt is discoverable, it also runs the desktop Debug configure/build.

Prepare the local development AI model:

```powershell
.\scripts\setup-ollama-model.ps1
```

By default this checks/pulls `gemma4:e4b`. For the smaller fallback model:

```powershell
.\scripts\setup-ollama-model.ps1 -Fallback
```

The Ollama script checks that Ollama is installed, verifies `http://localhost:11434`, lists installed models, asks before pulling, and runs the `LOCAL_JARVIS_READY` health check prompt.

## Developer vs End-User Setup

This document is for developers building Local Jarvis from source. CMake, vcpkg, Visual Studio Build Tools, and the Qt SDK are developer dependencies.

They are not intended to be end-user runtime requirements. A future Windows installer should ship the built application and required runtime DLLs so end users do not install CMake, vcpkg, Visual Studio Build Tools, or the Qt SDK. See [packaging-windows.md](packaging-windows.md).

## Prerequisites

- Windows 10 or newer.
- Visual Studio 2022 Build Tools with:
  - Desktop development with C++
  - MSVC v143 C++ x64/x86 build tools
  - Windows 10 or Windows 11 SDK
- CMake 3.22 or newer.
- Git, for cloning vcpkg if needed.
- vcpkg installed locally.
- Qt 6 for the desktop build only.

## vcpkg Setup

Clone and bootstrap vcpkg if you do not already have it:

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\src\vcpkg
cd C:\src\vcpkg
.\bootstrap-vcpkg.bat
```

Set `VCPKG_ROOT` in the shell where you run CMake:

```powershell
$env:VCPKG_ROOT = "C:\src\vcpkg"
```

The presets use:

```text
%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
```

vcpkg installs these manifest dependencies from `vcpkg.json`:

- `sqlite3`
- `nlohmann-json`

## Core-Only Build and Tests

Core presets set `LOCAL_JARVIS_BUILD_DESKTOP=OFF`, so they never require Qt:

```powershell
cd D:\Work-Projects\local-jarvis
$env:VCPKG_ROOT = "C:\src\vcpkg"

cmake --preset windows-msvc-core-debug
cmake --build --preset windows-msvc-core-debug
ctest --preset windows-msvc-core-debug
```

Release core build:

```powershell
cmake --preset windows-msvc-core-release
cmake --build --preset windows-msvc-core-release
ctest --preset windows-msvc-core-release
```

## Qt Desktop Prerequisites

Install Qt 6 separately. Use a Qt kit that matches MSVC 2022, for example:

```text
C:\Qt\6.7.0\msvc2019_64
```

Qt names the MSVC 64-bit kit `msvc2019_64` for current supported MSVC toolchains; that kit is the usual choice for Visual Studio 2022 builds. Use the path for your installed Qt version.

Set `LOCAL_JARVIS_QT_PREFIX` before configuring the desktop preset:

```powershell
$env:LOCAL_JARVIS_QT_PREFIX = "C:\Qt\6.7.0\msvc2019_64"
```

You can also pass the path directly with `CMAKE_PREFIX_PATH`:

```powershell
cmake --preset windows-msvc-desktop-debug -DCMAKE_PREFIX_PATH="C:\Qt\6.7.0\msvc2019_64"
```

Do not commit local Qt paths or `CMakeUserPresets.json`.

## Desktop Build

```powershell
cd D:\Work-Projects\local-jarvis
$env:VCPKG_ROOT = "C:\src\vcpkg"
$env:LOCAL_JARVIS_QT_PREFIX = "C:\Qt\6.7.0\msvc2019_64"

cmake --preset windows-msvc-desktop-debug
cmake --build --preset windows-msvc-desktop-debug
```

The desktop preset still uses vcpkg for core dependencies and only uses Qt for the `local-jarvis-desktop` target.

## Common Errors

`Could not find toolchain file`

Confirm `VCPKG_ROOT` is set and this file exists:

```text
%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
```

`Could NOT find SQLite3`

Use one of the presets so vcpkg installs `sqlite3`, or pass the vcpkg toolchain file manually.

`Could not find a package configuration file provided by "nlohmann_json"`

Use one of the presets so vcpkg installs `nlohmann-json`, or pass the vcpkg toolchain file manually.

`Could NOT find Qt6`

Qt is required only for `windows-msvc-desktop-debug`. Install Qt 6 with the MSVC 64-bit component, then set:

```powershell
$env:LOCAL_JARVIS_QT_PREFIX = "C:\Qt\<version>\msvc2019_64"
```

or pass:

```powershell
-DCMAKE_PREFIX_PATH="C:\Qt\<version>\msvc2019_64"
```

If you are not working on UI code, use the core presets instead.

`MSB8020` or missing Windows SDK/MSVC toolset

Open the Visual Studio Installer and install the C++ desktop workload, MSVC v143 tools, and a Windows SDK.
