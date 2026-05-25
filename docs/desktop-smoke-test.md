# Desktop Smoke Test

Use this checklist after a successful `windows-msvc-desktop-debug` build. It verifies the desktop scaffold, companion shell, caption pipeline, and Phase 3A microphone foundation. It does not require system audio capture, screen capture, OCR, ASR, cloud services, or downloaded models.

## Launch

- Start `local-jarvis-desktop.exe` from the desktop build output.
- Confirm the app opens without crashing.
- Confirm the Setup tab renders first.
- Confirm the Settings tab renders.
- Confirm the Session tab renders after database readiness. Model readiness should not be required for raw session work.
- Confirm the Microphone Input section renders with audio mode, device selector, refresh button, level meter, and error/status text.
- Confirm the Microphone Input section shows diagnostics: selected device name/id, active state, sample rate, channel count, sample format, buffer/frame counters, non-zero samples, RMS, smoothed level, callback time, and last error.
- Confirm the companion shell appears as a small always-on-top window.
- Confirm the caption bubble appears near the companion when captions are enabled.
- Confirm the companion shows the current mode's outfit/accessory labels.

## Session Flow

- With no capture permissions enabled, start a session.
- Confirm the session starts and shows an active session ID.
- Confirm microphone and system audio runtime state remains stopped unless explicitly enabled.
- Confirm the default audio mode is dummy audio unless a developer explicitly selected real microphone mode.
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
- Change caption mode to Off.
- Confirm the caption bubble hides or clears text.
- Change caption mode to Original only.
- Confirm original text is shown with the speaker label when enabled.
- Change caption mode to English only.
- Confirm translated English text is shown.
- Change caption mode to Original + English.
- Confirm original and translated lines are shown together.
- Change caption mode to Summary.
- Confirm summary text is shown.
- Toggle speaker labels.
- Confirm speaker labels hide and reappear.
- Adjust caption max lines and max characters in Settings.
- Confirm caption output respects the limits and settings persist after restart.
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

## Microphone Foundation

- Confirm real microphone capture is off at app launch.
- Click Refresh Devices.
- Confirm the microphone list loads real Windows input devices or shows a clear unavailable message.
- Switch Audio capture mode to Real microphone.
- Select a microphone device if devices are available.
- Click Test Mic Level.
- Confirm microphone capture starts only after that explicit click and no session is created.
- Confirm diagnostics update for buffers, frames, callback time, RMS, and smoothed level while the test runs.
- Speak into the selected microphone.
- Confirm the input level moves. If it stays at 0%, inspect diagnostics for sample format, frame count, non-zero sample count, and last error.
- Confirm the mic test stops after 10 seconds, or click Stop Mic Test and confirm it stops cleanly.
- Confirm privacy events are written for mic test start/stop.
- Start a session.
- Confirm the microphone remains stopped until the Microphone checkbox or assistant panel microphone toggle is explicitly clicked.
- Enable Microphone.
- Confirm the Capture Status panel shows the microphone as running.
- Confirm the input level meter moves when speaking into the selected microphone.
- Confirm diagnostics counters continue updating during session microphone capture.
- Confirm the companion switches to the Listening animation while microphone capture is active.
- Confirm the caption bubble shows a non-transcription placeholder such as "Mic active. Transcription will be added in the next phase."
- Disable Microphone.
- Confirm the microphone stops cleanly and the companion returns to its fallback animation state.
- Re-enable Microphone, then close the app while capture is active.
- Confirm the app exits cleanly.
- Confirm no raw audio files are created in the repo, build directories, or app data.
- Confirm local privacy events are written for microphone start, stop, and any start failure.
- Switch Audio capture mode back to Dummy audio.
- Confirm dummy captions and dummy transcript/session flow still work.

## Regression Boundaries

- Confirm no overlay UI appears.
- Confirm real microphone capture starts only after explicit user action.
- Confirm no system audio or screen capture starts.
- Confirm no OCR or ASR path runs.
- Confirm all capture status remains visible in the Session tab.
