# Local Jarvis

Local Jarvis is a local-first, privacy-focused desktop assistant for study sessions, meetings, and personal note workflows. The project is designed around explicit user control, local storage, and visible capture status.

This scaffold creates the C++20 architecture for a Qt 6 desktop application with SQLite-backed session storage, explicit microphone capture, a local ASR pipeline, and local model-processing hooks.

## Safety Scope

Local Jarvis does not implement stealth behavior, proctoring bypass, screen-recording evasion, monitoring evasion, or deceptive automation. The app must not hide from OBS, Zoom, screen sharing, screen recording, endpoint monitoring, or other visibility tools.

All future capture features must require explicit user permission and must show visible capture status in the desktop UI.

## Current Features

- Qt 6 desktop shell
- Start Session and Stop Session buttons
- Visible capture status panel
- Transcript and processed notes panels
- SQLite storage layer in the platform app-data directory
- Session lifecycle creation and stop timestamps
- Storage methods for sessions, transcript segments, processed notes, and recent session listing
- Dummy audio capture backend that generates fake transcript lines only after explicit user permission and session start
- Optional local ASR pipeline with `StubAsrEngine` by default and whisper.cpp support behind `LOCAL_JARVIS_ENABLE_WHISPER`
- Local Ollama setup flow for Gemma models at `http://localhost:11434`
- Background AI processing jobs for local study chunks, meeting chunks, and final summaries
- First-run setup and settings screens for database/model readiness
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
core/asr/            Local ASR pipeline, stub backend, optional Whisper backend
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

## Optional Local ASR

Local Jarvis does not use cloud ASR or background uploads. The default ASR engine is a stub.

To prepare whisper.cpp integration, provide a local checkout and configure with:

```powershell
cmake -S . -B build `
  -DLOCAL_JARVIS_ENABLE_WHISPER=ON `
  -DLOCAL_JARVIS_WHISPER_CPP_DIR="C:\dev\whisper.cpp"
```

See [docs/asr.md](docs/asr.md) for details.

## Local AI Setup

Local Jarvis prepares Ollama integration for local Gemma models only. It checks the local Ollama API, recommends `gemma4:e4b` on systems with at least 16 GB RAM, and falls back to `gemma4:e2b` on smaller systems.

The app never silently downloads models. Pulls are started only by user action from the setup/settings UI. See [docs/ollama.md](docs/ollama.md).

AI processing jobs run locally through Ollama. Study jobs store `study_chunk` notes and flashcards. Meeting jobs store `meeting_chunk` notes and action items. Invalid model JSON is stored as raw processed-note output and logged as a model event instead of crashing.

## Local Database

Local Jarvis stores its SQLite database at:

- Windows: `%LOCALAPPDATA%/LocalJarvis/data/local_jarvis.db`
- macOS: `~/Library/Application Support/LocalJarvis/data/local_jarvis.db`
- Linux: `~/.local/share/local-jarvis/data/local_jarvis.db`

The schema is migrated idempotently and records `schema_version` in the `settings` table. If SQLite FTS5 is available, transcript and processed-note search use local FTS tables.

## Status

This is still an architecture-first scaffold. It supports explicit microphone capture and local ASR backends, but it does not capture system audio, capture screens, run OCR, or call any cloud AI service. Raw audio is not written to disk.
