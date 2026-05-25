# Local ASR

Local Jarvis has a local-only ASR scaffold that accepts in-memory microphone PCM chunks, processes them off the UI thread, feeds the caption pipeline, and stores final transcript segments during active user-started sessions.

## Default Build

By default, `LOCAL_JARVIS_ENABLE_WHISPER` is `OFF`. The app builds with `StubAsrEngine`, and the desktop UI shows:

```text
ASR backend: Stub
```

The stub backend is deterministic for development and automated tests. It produces text such as:

```text
Stub transcript chunk 1
Stub transcript chunk 2
```

This is not real speech recognition. It verifies the local transcript pipeline without requiring a model download, microphone hardware, or `whisper.cpp`.

## Runtime Behavior

- Microphone capture still starts only after an explicit user action.
- The ASR toggle is separate from the microphone toggle.
- ASR chunks are buffered in memory only.
- Raw audio is not written to disk.
- Transcript segments are stored in SQLite only while an explicit session is active.
- Standalone microphone level tests do not create transcript records.
- Stub transcript segments use source `microphone_asr_stub`.

## Enable whisper.cpp Hooks

Add a local `whisper.cpp` checkout under:

```text
third_party/whisper.cpp
```

Then configure with:

```powershell
cmake -S . -B build -DLOCAL_JARVIS_ENABLE_WHISPER=ON
```

If `LOCAL_JARVIS_ENABLE_WHISPER` is `ON` and `LOCAL_JARVIS_WHISPER_SOURCE_DIR` does not point to a local `whisper.cpp` checkout, CMake fails with a clear message. The scaffold also expects the dependency to export a CMake target named `whisper`.

The current Whisper backend is a placeholder hook. Real transcription is not implemented in this phase.

## Safety Boundary

ASR is local-only. Local Jarvis does not use cloud ASR, background uploads, hidden capture, system audio capture, screen capture, OCR, or background transcription.
