# Local ASR

Local Jarvis has a local-only ASR pipeline that accepts in-memory microphone or system-audio PCM chunks, processes them off the UI thread, feeds captions, and stores final transcript text during active user-started sessions.

## Default Build: Stub

By default, `LOCAL_JARVIS_ENABLE_WHISPER` is `OFF`.

```powershell
cmake --preset windows-msvc-core-debug
cmake --build --preset windows-msvc-core-debug
ctest --preset windows-msvc-core-debug
```

In this build:

- Stub remains the default ASR backend.
- Whisper is shown as unavailable/disabled in the desktop UI.
- No Whisper headers, libraries, model files, downloads, or network services are required.
- Stub transcript segments use source `microphone_asr_stub` for microphone audio and `system_audio_asr_stub` for system audio.

The stub backend emits deterministic development text such as:

```text
Stub transcript chunk 1
```

## Build With whisper.cpp

Whisper support is opt-in and requires a local `whisper.cpp` checkout. Local Jarvis does not vendor `whisper.cpp`, does not download it during configure/build, and does not download model files.

Example:

```powershell
cmake -S . -B out/build/windows-msvc-core-whisper `
  -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
  -DLOCAL_JARVIS_BUILD_DESKTOP=OFF `
  -DLOCAL_JARVIS_ENABLE_WHISPER=ON `
  -DLOCAL_JARVIS_WHISPER_CPP_DIR="C:\dev\whisper.cpp"
cmake --build out/build/windows-msvc-core-whisper --config Debug
```

If `LOCAL_JARVIS_ENABLE_WHISPER=ON` and `LOCAL_JARVIS_WHISPER_CPP_DIR` does not point to a valid checkout containing `CMakeLists.txt`, CMake fails with a clear message. The checkout must export a CMake target named `whisper`.

## Model Files

Select a local Whisper model file in the desktop UI:

```text
Session > Local ASR > Backend: Whisper > Browse Model
```

Suggested development models:

- `tiny` or `base` for fast local testing.
- `small` for better quality once the pipeline is verified.

Do not commit model files to this repo.

## Runtime Behavior

- Microphone capture still starts only after explicit user action.
- System audio loopback still starts only after explicit user action.
- Microphone ASR and System Audio ASR are controlled separately from the capture toggles.
- Whisper chunks are classified before transcription as silence, too quiet, maybe speech, likely speech, or clipping risk.
- Clearly silent chunks are skipped before Whisper. Too-quiet chunks are skipped by default and shown as a visible warning instead of creating transcript rows.
- Empty, whitespace-only, and `[BLANK_AUDIO]` ASR outputs are suppressed by default and counted in diagnostics.
- Optional Whisper preprocessing is in-memory only. When enabled, it can apply capped gain to low but non-silent chunks and uses limiter protection to avoid clipping.
- Whisper processes normalized in-memory mono float samples.
- 48 kHz and other sample rates are converted to 16 kHz with a simple MVP linear resampler.
- Raw audio is not written to disk.
- Transcript text is stored only while an explicit session is active.
- Standalone microphone and system audio level tests do not store transcripts.
- Whisper transcript segments use source `microphone_asr_whisper` for microphone audio and `system_audio_asr_whisper` for system audio.
- Caption source labels distinguish inputs as `Mic:` and `System:` when source labels are enabled.
- Caption source display modes are available for combined chronological captions, system-only captions, mic-only captions, prefer-system captions, and prefer-mic captions. The default is combined chronological with source labels on.
- If microphone and system audio produce the same or nearly same text within the duplicate window, Local Jarvis suppresses the duplicate caption/transcript row and prefers system audio for online class or playback captions.
- Caption cleaning normalizes whitespace, collapses obvious repeated punctuation, lightly capitalizes/punctuates ASR text, and rejects known non-content tokens such as `[BLANK_AUDIO]`.
- Short consecutive captions from the same source/speaker can be merged in memory before display/storage quality checks. The default merge window is 1200 ms and does not merge microphone and system audio together.

## Audio And Caption Quality Settings

These settings are stored in SQLite and can be adjusted by development builds or future UI controls:

- `asr.speech.silence_dbfs_threshold`, default `-60.0`
- `asr.speech.too_quiet_dbfs_threshold`, default `-45.0`
- `asr.speech.likely_dbfs_threshold`, default `-32.0`
- `asr.speech.clipping_peak_threshold`, default `0.90`
- `asr.speech.min_non_zero_percentage`, default `0.02`
- `asr.speech.min_chunk_duration_ms`, default `300`
- `asr.debug_process_too_quiet`, default `false`
- `asr.preprocessing.enabled`, default `false`
- `asr.preprocessing.target_rms`, default `0.08`
- `asr.preprocessing.max_gain_db`, default `12.0`
- `caption.hold_ms`, default `4000`
- `caption.suppress_duplicates`, default `true`
- `caption.duplicate_window_ms`, default `5000`
- `caption.clear_on_asr_off`, default `false`
- `caption.source_labels.enabled`, default `true`
- `caption.source_display_mode`, default `Combined`
- `caption.cleaning.enabled`, default `true`
- `caption.merge_short_segments`, default `true`
- `caption.merge_max_gap_ms`, default `1200`
- `caption.merge_max_characters`, default `220`
- `caption.auto_punctuation_light`, default `true`

Caption display is stabilized by holding the last useful caption briefly, cleaning text before display, merging short same-source captions, suppressing repeated identical captions within the duplicate window, suppressing cross-source duplicates, and avoiding label refresh work when the formatted caption text has not changed.

## Local Events

Audio and ASR optimization paths write local diagnostic events only. They do not write raw audio:

- `asr_chunk_skipped_silence`
- `asr_chunk_skipped_too_quiet`
- `asr_blank_output`
- `asr_duplicate_suppressed`
- `asr_preprocessing_applied`
- `system_audio_asr_enabled`
- `system_audio_asr_disabled`
- `system_audio_asr_chunk_processed`
- `system_audio_asr_chunk_skipped_silence`
- `system_audio_asr_chunk_skipped_too_quiet`
- `system_audio_asr_blank_output`
- `system_audio_asr_error`

## Privacy Boundary

ASR is local-only. Local Jarvis does not use cloud ASR, background uploads, hidden capture, screen capture, OCR, or background transcription. Microphone and system audio are processed in memory and dropped; only non-empty, non-blank, non-duplicate transcript text from active sessions is stored in SQLite.
