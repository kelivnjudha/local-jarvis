# Local ASR

Local Jarvis is prepared for local speech-to-text through `whisper.cpp`, but the integration is optional and disabled by default.

## Default Build

By default, `LOCAL_JARVIS_ENABLE_WHISPER` is `OFF`. The app builds with `StubAsrEngine`, and the desktop UI shows:

```text
ASR engine: stub
```

The stub does not transcribe audio and does not produce real transcript text.

## Enable whisper.cpp Hooks

Add a local `whisper.cpp` checkout under:

```text
third_party/whisper.cpp
```

Then configure with:

```powershell
cmake -S . -B build -DLOCAL_JARVIS_ENABLE_WHISPER=ON
```

If `third_party/whisper.cpp/CMakeLists.txt` exists and exports a CMake target named `whisper`, Local Jarvis links `local_jarvis_core` against that target. If the directory or target is missing, the project still builds the `WhisperAsrEngine` stub and prints a CMake warning.

## Safety Boundary

ASR is local-only. Local Jarvis does not use cloud ASR, background uploads, hidden capture, or background transcription.

Future real ASR work must run only during active user-started sessions and only from audio sources allowed by `PrivacyManager`.
