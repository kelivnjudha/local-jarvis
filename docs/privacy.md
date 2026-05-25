# Privacy

Local Jarvis is local-first. The initial architecture assumes user data should remain on the user's machine unless a future feature explicitly documents otherwise and asks for consent.

## Default Capture State

All capture permissions default to disabled:

- Microphone: disabled
- System audio: disabled
- Screen capture: disabled

The desktop UI always shows capture status. Future capture controls must keep this status visible and understandable.

## Permission Model

Future capture features must require explicit user action before any microphone, system audio, or screen capture can start. Stubs currently return a permission-required status when capture has not been enabled.

Permission state should be treated as runtime state, not as a hidden global. Modules that capture or process user data should receive privacy state through explicit dependencies.

The dummy audio backend also follows this rule. It produces fake transcript lines only when the user-visible permission state is enabled and a session is active.

ASR is local-only. The default stub does not transcribe, upload, or call any network service. Future whisper.cpp integration must process local PCM during active user-started sessions only.

Ollama integration is also local-only. `OllamaClient` connects to `http://localhost:11434` and does not call cloud AI APIs. Model pulls happen only after a user-triggered setup/settings action.

AI processing jobs use local session transcripts and OCR text already stored in SQLite. They run through local Ollama only and do not upload transcript, OCR, notes, or prompts to a cloud API.

## Local Data

The desktop app stores session metadata in a local SQLite database under the user's app-data directory:

- Windows: `%LOCALAPPDATA%/LocalJarvis/data/local_jarvis.db`
- macOS: `~/Library/Application Support/LocalJarvis/data/local_jarvis.db`
- Linux: `~/.local/share/local-jarvis/data/local_jarvis.db`

The current schema stores sessions, transcript segments, OCR segments, processed notes, action items, flashcards, model events, privacy events, and settings locally.

Future storage work should include clear retention controls, export controls, and deletion paths.
