# Audio Capture

Local Jarvis has two local capture foundations:

- Microphone input captures the selected Windows input device after explicit user action.
- System audio loopback captures the selected Windows output device after explicit user action.

Both paths keep PCM in memory only for diagnostics and downstream local processing. Raw audio is not written to disk.

## Microphone

Microphone capture is used by the current local ASR scaffold. It can feed in-memory PCM chunks to Stub ASR or optional local Whisper during active user-started sessions.

## System Audio

Phase 3E-A adds Windows WASAPI loopback for desktop output audio. This phase is diagnostics-only:

- System audio is not fed into ASR.
- System audio is not transcribed.
- System audio does not create sessions.
- System audio does not store transcript rows.
- System audio does not write raw audio files.

Use the desktop Session tab's System Audio Output section to refresh output devices, choose an output device, and run a 10-second system audio test. The test records local privacy events such as `system_audio_test_started`, `system_audio_test_stopped`, and `system_audio_capture_failed`.

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
