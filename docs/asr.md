# Local ASR

Local Jarvis has a local-only ASR pipeline that accepts in-memory microphone PCM chunks, processes them off the UI thread, feeds captions, and stores final transcript text during active user-started sessions.

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
- Stub transcript segments use source `microphone_asr_stub`.

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
- ASR is controlled separately from the microphone toggle.
- Whisper processes normalized in-memory mono float samples.
- 48 kHz and other sample rates are converted to 16 kHz with a simple MVP linear resampler.
- Raw audio is not written to disk.
- Transcript text is stored only while an explicit session is active.
- Standalone microphone level tests do not store transcripts.
- Whisper transcript segments use source `microphone_asr_whisper`.

## Privacy Boundary

ASR is local-only. Local Jarvis does not use cloud ASR, background uploads, hidden capture, system audio capture, screen capture, OCR, or background transcription. Audio is processed in memory and dropped; only transcript text from active sessions is stored in SQLite.
