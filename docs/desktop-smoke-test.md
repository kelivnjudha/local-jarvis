# Desktop Smoke Test

Use this checklist after a successful `windows-msvc-desktop-debug` build. It verifies the existing desktop scaffold only; it does not require real audio capture, screen capture, OCR, ASR, or downloaded models.

## Launch

- Start `local-jarvis-desktop.exe` from the desktop build output.
- Confirm the app opens without crashing.
- Confirm the Setup tab renders first.
- Confirm the Settings tab renders.
- Confirm the Session tab renders after database readiness. Model readiness should not be required for raw session work.

## Session Flow

- With no capture permissions enabled, start a session.
- Confirm the session starts and shows an active session ID.
- Confirm microphone and system audio runtime state remains stopped unless explicitly enabled.
- Stop the session.
- Confirm the session stops without errors.
- Confirm raw session storage works even if the local model is unavailable.

## Model Readiness States

- Run with Ollama stopped or with the configured model missing.
- Confirm setup/settings show the model unavailable state clearly.
- Confirm starting and stopping a raw session is still possible when the database is ready.
- Stop a session while the model is unavailable.
- Confirm final summary status is marked as waiting for model readiness, not treated as a crash or capture failure.

## Worker Lifetime

- Start local AI setup or a model status/pull action.
- Close the app while background work is still pending.
- Confirm the app exits cleanly without an access violation.
- Reopen the app and repeat with the fallback model pull action if practical.
- Confirm no queued UI update touches a destroyed `MainWindow`.

## Regression Boundaries

- Confirm no companion UI appears.
- Confirm no overlay UI appears.
- Confirm no real microphone, system audio, or screen capture starts.
- Confirm no OCR or ASR path runs.
- Confirm all capture status remains visible in the Session tab.
