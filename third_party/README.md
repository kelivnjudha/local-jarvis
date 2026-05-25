# Third-Party Dependencies

Optional third-party source dependencies can be placed here.

For local speech-to-text, add `whisper.cpp` as:

```text
third_party/whisper.cpp
```

Local Jarvis does not vendor or require `whisper.cpp` by default. Enable the integration hooks with:

```powershell
cmake -S . -B build -DLOCAL_JARVIS_ENABLE_WHISPER=ON
```
