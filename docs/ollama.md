# Ollama + Gemma Setup

Local Jarvis uses Ollama only through the local API at:

```text
http://localhost:11434
```

There are no cloud AI calls, background uploads, or remote model providers in this integration.

## Models

Default model:

```text
gemma4:e4b
```

Fallback model:

```text
gemma4:e2b
```

`ModelManager::detectRecommendedModel()` recommends `gemma4:e4b` when system RAM is at least 16 GB. Otherwise it recommends `gemma4:e2b`.

## First-Run Setup

The setup screen checks:

- Local SQLite database
- Local Ollama availability
- Recommended Gemma model availability

Model downloads are never silent. If the model is missing, Local Jarvis shows setup status and waits for the user to press the setup button.

## Manual Commands

Install Ollama from the official Ollama installer, start it locally, then pull a model:

```powershell
ollama pull gemma4:e4b
```

Fallback:

```powershell
ollama pull gemma4:e2b
```

Delete a local model manually:

```powershell
ollama rm gemma4:e4b
```

The desktop settings screen displays this deletion instruction, but Local Jarvis does not delete models automatically.

## Health Check

After a user-triggered setup pull, Local Jarvis sends this local prompt to Ollama:

```text
Reply only with: LOCAL_JARVIS_READY
```

Setup is considered complete only when the local model replies with `LOCAL_JARVIS_READY`.
