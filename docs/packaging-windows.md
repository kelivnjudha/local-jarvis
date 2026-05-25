# Windows Packaging Plan

This is the future packaging direction for Local Jarvis. It is not implemented yet.

## Goals

- Ship a Release desktop app build.
- Include required runtime DLLs.
- Create Start Menu and desktop shortcuts.
- Keep first-run setup local and explicit.
- Do not require end users to install CMake, vcpkg, Visual Studio Build Tools, or the Qt SDK.

## Proposed Flow

1. Build the Release desktop app with the Windows desktop preset.
2. Use Qt deployment tooling, such as `windeployqt`, or an equivalent scripted deployment step to collect Qt runtime DLLs and plugins.
3. Include non-Qt runtime DLLs needed by the app, including the C++ runtime and any vcpkg-provided runtime libraries.
4. Package the deployment directory with Inno Setup or WiX.
5. Create app shortcuts during installation.
6. On first launch, initialize the local SQLite database in the user app-data directory.
7. Guide the user through local Ollama/model setup from the app UI.

## Developer Dependencies vs Runtime

Developers need:

- CMake
- vcpkg
- Visual Studio Build Tools
- Qt SDK

End users should receive:

- `local-jarvis-desktop.exe`
- Qt runtime DLLs and plugins
- Required Microsoft/vcpkg runtime DLLs
- Installer-created shortcuts
- First-run setup guidance inside Local Jarvis

## Non-Goals

- No cloud model provider packaging.
- No real audio capture packaging in the current scaffold.
- No screen capture, OCR, ASR, overlay, or companion UI packaging.
- No requirement that users install developer build tools.
