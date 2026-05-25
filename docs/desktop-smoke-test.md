# Desktop Smoke Test

Use this checklist after a successful `windows-msvc-desktop-debug` build. It verifies the desktop scaffold plus Companion Phase 1 shell only; it does not require real audio capture, screen capture, OCR, ASR, or downloaded models.

## Launch

- Start `local-jarvis-desktop.exe` from the desktop build output.
- Confirm the app opens without crashing.
- Confirm the Setup tab renders first.
- Confirm the Settings tab renders.
- Confirm the Session tab renders after database readiness. Model readiness should not be required for raw session work.
- Confirm the companion shell appears as a small always-on-top window.
- Confirm the caption bubble appears near the companion when captions are enabled.
- Confirm the companion shows the current mode's outfit/accessory labels.

## Session Flow

- With no capture permissions enabled, start a session.
- Confirm the session starts and shows an active session ID.
- Confirm microphone and system audio runtime state remains stopped unless explicitly enabled.
- Stop the session.
- Confirm the session stops without errors.
- Confirm raw session storage works even if the local model is unavailable.

## Companion Shell

- Click the companion.
- Confirm the compact assistant panel opens.
- Confirm the companion briefly switches to the salute animation state.
- Change the mode in the panel.
- Confirm the mode label and Settings status update.
- Confirm the companion visual color, outfit label, and accessory label update for the selected mode.
- Confirm the caption bubble style changes for the selected mode.
- Confirm the animation state label changes after companion click, panel action, microphone placeholder toggle, or caption update.
- Toggle captions off and on.
- Confirm the caption bubble hides and reappears.
- Toggle translation.
- Confirm the Settings status updates.
- Adjust the companion scale slider.
- Confirm the companion size changes and the scale persists after restart.
- Toggle animation off.
- Confirm the companion returns to a still idle pose.
- Toggle animation on.
- Confirm panel/caption actions update the animation label again.
- Toggle idle motion.
- Confirm the Settings status updates.
- Drag the companion while it is unlocked.
- Confirm the companion moves and the position persists after restart.
- Use Settings > Hide Companion.
- Confirm the companion, panel, and captions hide.
- Use Settings > Show Companion.
- Confirm the companion returns.
- Use Settings > Reset Companion Position.
- Confirm the companion returns to the default anchor.
- Use Settings > Reset Visual Profile / Theme.
- Confirm scale, animation, idle motion, and always-on-top return to defaults.

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
- Close the app while the companion, caption bubble, and assistant panel are visible.
- Confirm no companion worker or timer touches destroyed UI.

## Regression Boundaries

- Confirm no overlay UI appears.
- Confirm no real microphone, system audio, or screen capture starts.
- Confirm no OCR or ASR path runs.
- Confirm all capture status remains visible in the Session tab.
