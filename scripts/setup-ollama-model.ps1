[CmdletBinding()]
param(
    [ValidateSet('gemma4:e4b', 'gemma4:e2b')]
    [string]$Model = 'gemma4:e4b',
    [switch]$Fallback,
    [switch]$Yes
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($Fallback) {
    $Model = 'gemma4:e2b'
}

function Write-Section {
    param([string]$Title)
    Write-Host ''
    Write-Host "== $Title =="
}

function Confirm-Step {
    param([string]$Prompt)
    if ($Yes) {
        return $true
    }
    $answer = Read-Host "$Prompt [y/N]"
    return $answer -match '^(y|yes)$'
}

function Invoke-Checked {
    param(
        [string]$Name,
        [string]$FilePath,
        [string[]]$Arguments
    )

    Write-Host "> $FilePath $($Arguments -join ' ')"
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE."
    }
}

Write-Section 'Ollama checks'

$ollama = Get-Command 'ollama' -ErrorAction SilentlyContinue
if (!$ollama) {
    throw 'Ollama was not found on PATH. Install Ollama, start it locally, then rerun this script.'
}
Write-Host "Ollama CLI: $($ollama.Source)"

try {
    $tags = Invoke-RestMethod -Uri 'http://localhost:11434/api/tags' -TimeoutSec 5
} catch {
    throw 'Ollama did not respond at http://localhost:11434. Start Ollama locally, then rerun this script.'
}

$installedModels = @()
if ($tags.models) {
    $installedModels = @($tags.models | ForEach-Object { $_.name })
}

Write-Host 'Installed models:'
if ($installedModels.Count -eq 0) {
    Write-Host '  (none)'
} else {
    foreach ($name in $installedModels) {
        Write-Host "  $name"
    }
}

$targetModel = $Model
if ($installedModels -notcontains $targetModel) {
    Write-Warning "$targetModel is not installed."
    if (Confirm-Step "Run 'ollama pull $targetModel'?") {
        Invoke-Checked "pull $targetModel" 'ollama' @('pull', $targetModel)
    } elseif ($targetModel -eq 'gemma4:e4b' -and (Confirm-Step "Pull fallback model gemma4:e2b instead?")) {
        $targetModel = 'gemma4:e2b'
        if ($installedModels -notcontains $targetModel) {
            Invoke-Checked "pull $targetModel" 'ollama' @('pull', $targetModel)
        }
    } else {
        Write-Warning 'No model was pulled. Local AI development setup is incomplete.'
        exit 1
    }
}

Write-Section 'Health check'

$prompt = 'Reply only with: LOCAL_JARVIS_READY'
$body = @{
    model = $targetModel
    prompt = $prompt
    stream = $false
} | ConvertTo-Json

$response = Invoke-RestMethod -Uri 'http://localhost:11434/api/generate' -Method Post -ContentType 'application/json' -Body $body -TimeoutSec 120
$reply = ''
if ($null -ne $response.response) {
    $reply = [string]$response.response
}

Write-Host "Model: $targetModel"
Write-Host "Reply: $($reply.Trim())"

if ($reply.Trim() -eq 'LOCAL_JARVIS_READY') {
    Write-Host 'Health check: passed'
    exit 0
}

Write-Warning 'Health check returned unexpected output.'
exit 1
