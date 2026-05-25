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

## Local Data

The desktop app stores session metadata in a local SQLite database at `./data/local_jarvis.db`. The current schema stores session timestamps and reserves tables for future transcript, OCR, notes, action item, flashcard, and privacy event data.

Future storage work should include clear retention controls, export controls, and deletion paths.
