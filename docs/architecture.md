# Architecture

Local Jarvis is organized as a Qt desktop shell on top of a modular C++ core. The core is intentionally UI-agnostic so future CLI, service, or test harness entry points can reuse the same session, storage, privacy, and processing modules.

## Layers

- `apps/desktop`: Qt 6 Widgets application and visible user controls.
- `core/ai`: Local Ollama client, Gemma model manager, and prompt helpers.
- `core/processing`: Background local AI processing queue and study/meeting processors.
- `core/setup`: First-run checks for local database, Ollama, and model readiness.
- `core/session`: Starts and stops local sessions, emits lifecycle events, and records session IDs.
- `core/storage`: Owns the SQLite connection, schema creation, and session persistence.
- `core/privacy`: Tracks explicit capture permissions and exposes capture status.
- `core/audio` and `core/screen`: Cross-platform interfaces. Windows microphone capture and Windows system-audio loopback diagnostics are implemented through explicit user action; screen capture remains out of scope.
- `core/asr`, `core/ocr`, and `core/llm`: Local processing boundaries. ASR defaults to `StubAsrEngine`; whisper.cpp support is optional behind `LOCAL_JARVIS_ENABLE_WHISPER`.
- `core/logging`: Minimal process-local lifecycle logging.

## Session Flow

1. The user clicks Start Session.
2. `SessionManager` asks `Storage` to create a session in local SQLite.
3. `Storage` generates a local session ID and inserts a row in `sessions`.
4. The desktop UI shows the active session and capture status.
5. The user clicks Stop Session.
6. `Storage` updates `ended_at`.

Microphone and system-audio capture are explicit and visible. Raw audio is kept in memory for diagnostics and local processing only and is not written to disk. In Phase 3E-A, system audio is diagnostics-only and is not fed into ASR or transcript storage.

Dummy transcript generation is available for testing the session, UI, and storage path. Stub ASR can also generate deterministic local transcript segments. Real Whisper ASR is optional, local-only, and stores transcript text only during active user-started sessions.

## Local AI

Ollama integration is local-only and targets `http://localhost:11434`. `OllamaClient` supports `/api/tags`, `/api/generate`, and `/api/chat` with non-streaming responses first.

`ModelManager` recommends `gemma4:e4b` when RAM is at least 16 GB and `gemma4:e2b` otherwise. `ensureModelReady()` checks status only; it does not download models. Model pulls run through explicit user-triggered setup/settings actions.

`ProcessingQueue` runs local AI jobs on a worker thread so the UI stays responsive. Jobs call Ollama locally, save model lifecycle events, and persist generated notes, flashcards, and action items. If the model returns invalid JSON, Local Jarvis stores the raw output with a null `json_body` and logs `json_parse_failed`.

## Platform Backends

The public capture interfaces are platform-neutral. Future implementation work should live behind the existing interfaces and use platform-specific files such as:

- `core/audio/AudioCapture_win.cpp`
- `core/audio/AudioCapture_macos.cpp`
- `core/audio/AudioCapture_linux.cpp`
- `core/screen/ScreenCapture_win.cpp`
- `core/screen/ScreenCapture_macos.cpp`
- `core/screen/ScreenCapture_linux.cpp`

These files are currently placeholders. Real capture backends must consult `PrivacyManager` before starting and must report visible status to the UI.

## Storage

SQLite is used for local persistence in the platform app-data directory:

- Windows: `%LOCALAPPDATA%/LocalJarvis/data/local_jarvis.db`
- macOS: `~/Library/Application Support/LocalJarvis/data/local_jarvis.db`
- Linux: `~/.local/share/local-jarvis/data/local_jarvis.db`

The schema is migrated idempotently and stores `schema_version` in `settings`. If SQLite FTS5 is available, Local Jarvis creates `transcript_fts` and `processed_notes_fts` for local search.

The current schema includes:

- `sessions`: session ID, mode, title, start/end timestamps, summary status.
- `transcript_segments`: transcript text linked to sessions.
- `screen_ocr_segments`: OCR text linked to sessions.
- `processed_notes`: generated local summaries, notes, and optional JSON bodies.
- `action_items`: extracted tasks linked to sessions.
- `flashcards`: study cards linked to sessions.
- `model_events`: local model setup and health-check events.
- `privacy_events`: explicit privacy and capture-state events.
- `settings`: schema/setup/current-model settings.

The schema is documented in `core/storage/Schema.sql` and mirrored by the storage initialization SQL.
