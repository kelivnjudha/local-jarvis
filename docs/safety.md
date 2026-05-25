# Safety

Local Jarvis is a visible local assistant, not a stealth tool.

## Out of Scope

The project must not implement:

- Stealth behavior
- Proctoring bypass
- Screen-recording evasion
- Monitoring evasion
- Hidden overlays
- Deceptive automation
- Features that hide the app from OBS, Zoom, screen sharing, recording, or monitoring tools
- Cloud ASR, background upload, or background transcription without an active user-started session
- Cloud AI calls or remote model providers

## Required Behavior

All future capture features must:

- Require explicit user permission
- Show visible capture status
- Stop when the user asks them to stop
- Keep logs or events for session lifecycle changes
- Respect operating system permission prompts and privacy controls

## Engineering Rule

When a feature touches audio, screen content, OCR, transcripts, or LLM processing, design it around consent, visible state, and local control first. If a feature would make Local Jarvis harder for a user or meeting participant to notice, inspect, or stop, it does not belong in this project.

Local model setup must remain explicit. Status checks may detect Ollama and local models, but model downloads require user action.
