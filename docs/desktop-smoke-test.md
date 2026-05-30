# Desktop Smoke Test

Use this checklist after a successful `windows-msvc-desktop-debug` build. It verifies the desktop scaffold, companion shell, caption pipeline, microphone foundation, system audio loopback foundation, and local ASR scaffold. It does not require screen capture, OCR, cloud services, whisper.cpp, or downloaded models.

## Launch

- Start `local-jarvis-desktop.exe` from the desktop build output.
- Confirm the app opens without crashing.
- Confirm the Setup tab renders first.
- Confirm the Settings tab renders.
- Confirm the Session tab renders after database readiness. Model readiness should not be required for raw session work.
- Confirm the Microphone Input section renders with audio mode, device selector, refresh button, level meter, and error/status text.
- Confirm the Microphone Input section shows diagnostics: selected device name/id, active state, sample rate, channel count, sample format, buffer/frame counters, non-zero samples, RMS, smoothed level, callback time, and last error.
- Confirm the System Audio Output section renders with output device selector, refresh button, Start System Audio Test button, System Audio ASR toggle, level meter, diagnostics, quality label, callback time, ASR counters, last system transcript, and last error.
- Confirm the Local ASR section renders with backend, toggle, status, queued/processed chunk counts, last transcript, and last error.
- Confirm the Local ASR section renders audio quality diagnostics: speech detector state, skipped silence count, skipped too-quiet count, blank output count, duplicate suppression count, preprocessing state, and last preprocessing gain.
- Confirm the Local ASR section renders Whisper settings: model path, Browse Model, language, translate toggle, max threads, and Whisper status.
- Confirm the companion shell appears as a small always-on-top window.
- Confirm the assistant panel is open by default near the companion.
- Confirm the caption bubble appears when captions are enabled.
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
- Confirm the compact assistant panel collapses/closes while the companion remains visible.
- Click the companion again.
- Confirm the compact assistant panel opens near the companion.
- Leave the app idle for at least 10 seconds.
- Confirm the assistant panel remains stable without visible flicker, repeated repositioning, or focus stealing.
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
- Toggle source labels.
- Confirm `Mic:` and `System:` source labels hide and reappear independently from speaker labels.
- Change caption source display mode to Combined, System only, Mic only, Prefer system, and Prefer mic.
- Confirm the caption bubble filters or prefers the selected source without hiding the companion or assistant panel.
- Toggle Clean captions.
- Confirm obvious whitespace/punctuation artifacts are cleaned when enabled and no `[BLANK_AUDIO]` text appears as a caption.
- Toggle Merge short captions.
- Confirm short same-source/same-speaker captions can merge cleanly, while microphone and system audio remain source-separated.
- Confirm caption quality counters update for accepted, rejected, merged, duplicate-suppressed, and no-op refresh cases.
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
- Confirm the companion can move anywhere on the visible desktop area and the position persists after restart.
- Confirm dragging the companion does not toggle the assistant panel.
- Confirm the assistant panel repositions only after the companion drag ends or when the panel is reopened.
- Use Settings > Close Assistant Panel.
- Confirm the assistant panel closes and the companion remains visible.
- Use Settings > Open Assistant Panel.
- Confirm the panel returns near the current companion position.
- Use Settings > Reset Robot Position.
- Confirm the companion returns to a safe default corner.
- Drag the caption bubble independently from the companion while caption placement is unlocked.
- Confirm the caption bubble position persists after restart.
- Resize the caption bubble with the corner handle.
- Confirm caption text reflows and the caption size persists after restart.
- Confirm dragging or resizing the caption bubble does not flicker, hide, show, or reposition the assistant panel.
- Use Settings > Lock caption position.
- Confirm caption dragging/resizing is disabled until unlocked again.
- Use Settings > Reset Caption Position.
- Confirm caption position and size return to a safe default.
- Use Settings > Reset Visual Profile / Theme.
- Confirm scale, animation, idle motion, and always-on-top return to defaults.
- Change a companion or caption setting.
- Confirm settings changes update the visible UI without assistant panel flicker.

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
- Confirm the caption bubble shows a clear microphone placeholder when ASR is off, such as "Mic active. Transcription is off."
- Disable Microphone.
- Confirm the microphone stops cleanly and the companion returns to its fallback animation state.
- Re-enable Microphone, then close the app while capture is active.
- Confirm the app exits cleanly.
- Confirm no raw audio files are created in the repo, build directories, or app data.
- Confirm local privacy events are written for microphone start, stop, and any start failure.
- Switch Audio capture mode back to Dummy audio.
- Confirm dummy captions and dummy transcript/session flow still work.

## System Audio Foundation

- Confirm system audio capture is off at app launch.
- Click Refresh Outputs.
- Confirm the output device list loads real Windows output devices or shows a clear unavailable message.
- Select the active playback device if devices are available.
- Click Start System Audio Test.
- Confirm system audio loopback starts only after that explicit click and no session is created.
- Play a local test tone, video, music, or browser audio through the selected output device.
- Confirm the system audio level meter and diagnostics show buffers, frames, callback time, RMS, peak, non-zero percentage, dBFS, and quality label.
- Confirm the caption bubble may show "System audio active. Transcription is off." when microphone capture is not active and System Audio ASR is off.
- Confirm the companion can switch to Listening while system audio capture is active.
- Confirm the system audio test stops after its configured duration, or click Stop System Audio Test and confirm it stops cleanly.
- Start a system audio test, then close the app while capture is active.
- Confirm the app exits cleanly.
- Confirm no raw audio files are created in the repo, build directories, or app data.
- Confirm local privacy events are written for system audio test start, stop, and capture failure when applicable.
- Confirm the standalone system audio test does not create a session or transcript row.

## System Audio ASR

- Confirm System Audio ASR is OFF by default unless a developer previously enabled it.
- Start a session.
- Select the active playback output device.
- Enable the System audio capture checkbox explicitly.
- Enable System Audio ASR explicitly.
- With Stub backend selected, play local audio through the selected output device.
- Confirm system audio diagnostics show non-zero playback levels.
- Confirm System Audio ASR counters update for queued/handled chunks.
- Confirm the caption bubble shows stub transcript text with a `System:` source label when source labels are enabled.
- Confirm the Transcript panel shows the same stub system transcript segment.
- Confirm SQLite stores a transcript segment with source `system_audio_asr_stub`.
- Confirm skipped silence, too-quiet chunks, and blank output counters are visible and do not flicker blank captions.
- Confirm short system captions merge when the merge toggle is enabled and stay separate when it is disabled.
- Disable System Audio ASR.
- Confirm system audio capture can remain controlled independently.
- Stop the session.
- Confirm system audio ASR and loopback capture stop cleanly.
- Enable microphone ASR separately.
- Confirm microphone ASR still stores `microphone_asr_stub` or `microphone_asr_whisper` and uses the `Mic:` source label.
- Enable microphone ASR and System Audio ASR together with the Stub backend.
- Confirm Combined mode shows both sources chronologically and the Mic-only/System-only modes filter correctly.
- If both sources emit the same or nearly same text, confirm the cross-source duplicate suppression counter increments or rely on the automated test coverage for that path.
- Confirm duplicate cross-source rows are not stored and system audio remains the preferred row for duplicate playback captions.
- Close the app while system audio capture or ASR is active.
- Confirm the app exits cleanly.
- Confirm no raw audio files are created in the repo, build directories, or app data.
- Confirm local privacy events are written for `system_audio_asr_enabled`, `system_audio_asr_disabled`, `system_audio_asr_chunk_processed`, skipped chunks, blank output, and error cases when applicable.

## Local ASR Scaffold

- Confirm ASR is OFF by default unless the developer previously enabled it.
- Confirm the ASR backend shows Stub when `LOCAL_JARVIS_ENABLE_WHISPER=OFF`.
- Confirm Whisper appears unavailable when `LOCAL_JARVIS_ENABLE_WHISPER=OFF`.
- Enable ASR without enabling the microphone.
- Confirm ASR status is visible and no microphone capture starts automatically.
- Run Test Mic Level with ASR enabled.
- Confirm the mic test does not create a session or transcript segment.
- Start a session.
- Enable Microphone explicitly.
- Enable ASR if it is not already enabled.
- Confirm ASR status moves through Listening or Processing while microphone PCM is active.
- Confirm the caption bubble shows stub transcript text such as "Stub transcript chunk 1".
- Confirm the caption bubble does not flicker blank between ASR chunks or skipped chunks.
- Confirm caption cleaning adds only light punctuation/capitalization and does not damage technical words or acronyms.
- Confirm the Transcript panel shows the same stub transcript segment.
- Confirm SQLite stores a transcript segment with source `microphone_asr_stub`.
- Disable ASR.
- Confirm microphone capture can remain controlled independently.
- Stop the session.
- Confirm ASR stops cleanly and does not process new chunks after session stop.
- Re-enable ASR and close the app while capture is active.
- Confirm the app exits cleanly.
- Confirm no raw audio files are created in the repo, build directories, or app data.
- Confirm privacy events are written for ASR enable, disable, chunk processed, and error cases when applicable.

## Whisper ASR Manual Check

Only run this section when Local Jarvis is built with `LOCAL_JARVIS_ENABLE_WHISPER=ON`, `LOCAL_JARVIS_WHISPER_CPP_DIR` points to a local `whisper.cpp` checkout, and a local model file is available.

- Select ASR backend Whisper.
- Select a local model file with Browse Model.
- Choose language Auto or a known language.
- Confirm Whisper status changes from model missing to model selected, then loading/ready after ASR starts.
- Start a session.
- Enable ASR.
- Enable Microphone explicitly.
- Speak a short phrase.
- Confirm too-quiet input shows "Input may be too quiet for transcription" instead of creating a normal transcript row.
- Confirm stronger input reaches ASR and the speech detector state moves to maybe speech, speech likely, or clipping risk.
- Confirm transcript text appears in the caption bubble.
- Confirm `[BLANK_AUDIO]`, empty, or whitespace-only ASR output is not shown as a normal caption.
- Confirm the Transcript panel shows the same text.
- Confirm SQLite stores a transcript segment with source `microphone_asr_whisper`.
- If System Audio ASR is enabled with Whisper, confirm playback speech either creates source `system_audio_asr_whisper` or reports a clear missing-model/unavailable/too-quiet status.
- Confirm SQLite does not store `[BLANK_AUDIO]`, empty, whitespace-only, or short-window duplicate transcript rows by default.
- If ASR preprocessing is enabled through settings, confirm the UI shows the last applied gain and limiter state, and no audio file is written.
- Stop the session.
- Confirm ASR and microphone stop cleanly.
- Close the app.
- Confirm no raw audio files or model files were written into the repo.
- Confirm local events are written for `whisper_model_load_started`, `whisper_model_load_completed` or `whisper_model_load_failed`, `whisper_transcript_segment_created`, and `asr_backend_changed`.

## Regression Boundaries

- Confirm no overlay UI appears.
- Confirm real microphone capture starts only after explicit user action.
- Confirm no system audio, microphone, or ASR path starts without explicit user action.
- Confirm no screen capture starts.
- Confirm no OCR path runs.
- Confirm no cloud ASR path runs.
- Confirm all capture status remains visible in the Session tab.
