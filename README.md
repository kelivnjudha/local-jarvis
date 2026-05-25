# Local Jarvis

Local Jarvis is a local-first, privacy-focused desktop assistant for study sessions, meetings, and personal note workflows. The project is designed around explicit user control, local storage, and visible capture status.

This initial scaffold creates the C++20 architecture for a Qt 6 desktop application with SQLite-backed session storage. Audio capture, screen capture, ASR, OCR, and local LLM processing are intentionally represented as clean interfaces and stubs only.

## Safety Scope

Local Jarvis does not implement stealth behavior, proctoring bypass, screen-recording evasion, monitoring evasion, or deceptive automation. The app must not hide from OBS, Zoom, screen sharing, screen recording, endpoint monitoring, or other visibility tools.

All future capture features must require explicit user permission and must show visible capture status in the desktop UI.

## Current Features

- Qt 6 desktop shell
- Start Session and Stop Session buttons
- Visible capture status panel
- Transcript and processed notes placeholder panels
- SQLite storage layer at `./data/local_jarvis.db`
- Session lifecycle creation and stop timestamps
- Storage methods for sessions, transcript segments, processed notes, and recent session listing
- Dummy audio capture backend that generates fake transcript lines only after explicit user permission and session start
- Privacy manager with microphone, system audio, and screen capture disabled by default
- Modular C++20 core for future local ASR, OCR, LLM, audio, and screen modules
- Platform-specific placeholder files for future capture backends

## Project Layout

```text
apps/desktop/        Qt desktop app
core/session/        Session lifecycle state
core/storage/        SQLite database access and schema
core/privacy/        Capture permission/status model
core/audio/          Audio capture interface and stubs
core/screen/         Screen capture interface and stubs
core/asr/            Local ASR placeholder interface
core/ocr/            Local OCR placeholder interface
core/llm/            Local LLM placeholder interface
core/logging/        Minimal lifecycle logging
docs/                Architecture, privacy, and safety notes
tests/               CMake test scaffold
```

## Build

Install a C++20 compiler, CMake 3.22 or newer, Qt 6 Widgets, and SQLite development libraries.

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

If CMake cannot find Qt 6, pass `CMAKE_PREFIX_PATH` to your Qt installation:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:\Qt\6.7.0\msvc2019_64"
```

To build only the core library and tests in an environment without Qt 6:

```powershell
cmake -S . -B build-core -DLOCAL_JARVIS_BUILD_DESKTOP=OFF
cmake --build build-core
ctest --test-dir build-core
```

## Status

This is an architecture-first scaffold. It does not capture real audio, capture screens, transcribe speech, run OCR, or run an LLM yet. Dummy audio capture generates fake transcript events for UI and storage testing only.
