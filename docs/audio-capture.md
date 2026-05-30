# Audio Capture

Local Jarvis has two local capture foundations:

- Microphone input captures the selected Windows input device after explicit user action.
- System audio loopback captures the selected Windows output device after explicit user action.

Both paths keep PCM in memory only for diagnostics and downstream local processing. Raw audio is not written to disk.

## Microphone

Microphone capture is used by the current local ASR scaffold. It can feed in-memory PCM chunks to Stub ASR or optional local Whisper during active user-started sessions.

## System Audio

Windows WASAPI loopback captures the selected desktop output device after explicit user action. It supports two visible flows:

- Standalone diagnostics/test: shows scalar levels and device details without creating a session or transcript rows.
- Active-session ASR: feeds in-memory PCM into the same local ASR pipeline used by microphone capture.

System audio ASR is controlled separately from microphone ASR. During an active user-started session, enabling system audio capture plus System Audio ASR can create transcript rows with source `system_audio_asr_stub` or `system_audio_asr_whisper`. If no session is active, system audio diagnostics can run, but transcript text is not stored.

Use the desktop Session tab's System Audio Output section to refresh output devices, choose an output device, run a 10-second system audio test, and enable System Audio ASR. The test records local privacy events such as `system_audio_test_started`, `system_audio_test_stopped`, and `system_audio_capture_failed`. ASR records local events such as `system_audio_asr_enabled`, `system_audio_asr_disabled`, `system_audio_asr_chunk_processed`, `system_audio_asr_chunk_skipped_silence`, `system_audio_asr_chunk_skipped_too_quiet`, `system_audio_asr_blank_output`, and `system_audio_asr_error`.

## Diagnostics

The UI shows scalar diagnostics only:

- selected output device name and id
- capture active state
- sample rate, channel count, and sample format
- buffers and frames captured
- non-zero sample count and percentage
- RMS, peak, RMS dBFS, and peak dBFS
- quality label
- last callback time
- last error

These values are enough to verify that Windows loopback capture is working without storing audio.
