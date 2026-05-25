# Architecture

Local Jarvis is organized as a Qt desktop shell on top of a modular C++ core. The core is intentionally UI-agnostic so future CLI, service, or test harness entry points can reuse the same session, storage, privacy, and processing modules.

## Layers

- `apps/desktop`: Qt 6 Widgets application and visible user controls.
- `core/session`: Starts and stops local sessions, emits lifecycle events, and records session IDs.
- `core/storage`: Owns the SQLite connection, schema creation, and session persistence.
- `core/privacy`: Tracks explicit capture permissions and exposes capture status.
- `core/audio` and `core/screen`: Cross-platform interfaces with OS-specific placeholder source files. `DummyAudioCapture` generates fake transcript events for testing.
- `core/asr`, `core/ocr`, and `core/llm`: Local processing boundaries for future engines.
- `core/logging`: Minimal process-local lifecycle logging.

## Session Flow

1. The user clicks Start Session.
2. `SessionManager` asks `Storage` to create a session in local SQLite.
3. `Storage` generates a local session ID and inserts a row in `sessions`.
4. The desktop UI shows the active session and capture status.
5. The user clicks Stop Session.
6. `Storage` updates `ended_at`.

No audio or screen data is captured in this scaffold.

Dummy transcript generation is available only for testing the session, UI, and storage path. It starts after a session is active and the relevant `PrivacyManager` permission is enabled.

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

SQLite is used for local persistence at `./data/local_jarvis.db`. The initial schema includes:

- `sessions`: session ID, mode, start timestamp, optional end timestamp, optional title.
- `transcript_segments`: future transcript text linked to sessions.
- `screen_ocr_segments`: future OCR text linked to sessions.
- `processed_notes`: generated local summaries, notes, or other note artifacts.
- `action_items`: future extracted tasks linked to sessions.
- `flashcards`: future study cards linked to sessions.
- `privacy_events`: future explicit privacy and capture-state events.

The schema is documented in `core/storage/Schema.sql` and mirrored by the storage initialization SQL.
