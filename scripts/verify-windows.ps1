[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$env:VCPKG_DISABLE_METRICS = '1'

$results = [ordered]@{}

function Write-Section {
    param([string]$Title)
    Write-Host ''
    Write-Host "== $Title =="
}

function Invoke-VerificationStep {
    param(
        [string]$Name,
        [string]$FilePath,
        [string[]]$Arguments
    )

    Write-Host "> $FilePath $($Arguments -join ' ')"
    & $FilePath @Arguments
    if ($LASTEXITCODE -eq 0) {
        $script:results[$Name] = 'passed'
        return
    }

    $script:results[$Name] = "failed ($LASTEXITCODE)"
    throw "$Name failed with exit code $LASTEXITCODE."
}

function Test-QtPrefix {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $false
    }

    $expanded = [Environment]::ExpandEnvironmentVariables($Path.Trim('"'))
    if (!(Test-Path $expanded)) {
        return $false
    }

    return (Test-Path (Join-Path $expanded 'lib\cmake\Qt6\Qt6Config.cmake')) -or
        (Test-Path (Join-Path $expanded 'lib\cmake\Qt6'))
}

function Find-QtPrefix {
    if (Test-QtPrefix $env:LOCAL_JARVIS_QT_PREFIX) {
        return [Environment]::ExpandEnvironmentVariables($env:LOCAL_JARVIS_QT_PREFIX.Trim('"'))
    }

    if (![string]::IsNullOrWhiteSpace($env:CMAKE_PREFIX_PATH)) {
        foreach ($candidate in ($env:CMAKE_PREFIX_PATH -split ';')) {
            if (Test-QtPrefix $candidate) {
                return [Environment]::ExpandEnvironmentVariables($candidate.Trim('"'))
            }
        }
    }

    if (Test-Path 'C:\Qt') {
        $candidate = Get-ChildItem -Path 'C:\Qt' -Directory -ErrorAction SilentlyContinue |
            ForEach-Object {
                Get-ChildItem -Path $_.FullName -Directory -Filter 'msvc*_64' -ErrorAction SilentlyContinue
            } |
            Where-Object { Test-QtPrefix $_.FullName } |
            Sort-Object FullName -Descending |
            Select-Object -First 1

        if ($candidate) {
            return $candidate.FullName
        }
    }

    return $null
}

if ([string]::IsNullOrWhiteSpace($env:VCPKG_ROOT)) {
    throw 'VCPKG_ROOT is not set. Run scripts/bootstrap-windows.ps1 or set VCPKG_ROOT before verification.'
}

$toolchain = Join-Path $env:VCPKG_ROOT 'scripts\buildsystems\vcpkg.cmake'
if (!(Test-Path $toolchain)) {
    throw "vcpkg toolchain file was not found: $toolchain"
}

$qtPrefix = Find-QtPrefix
if ($qtPrefix) {
    $env:LOCAL_JARVIS_QT_PREFIX = $qtPrefix
}

Push-Location $repoRoot
try {
    Write-Section 'Core Debug'
    Invoke-VerificationStep 'core debug configure' 'cmake' @('--preset', 'windows-msvc-core-debug')
    Invoke-VerificationStep 'core debug build' 'cmake' @('--build', '--preset', 'windows-msvc-core-debug')
    Invoke-VerificationStep 'core debug tests' 'ctest' @('--preset', 'windows-msvc-core-debug')

    Write-Section 'Core Release'
    Invoke-VerificationStep 'core release configure' 'cmake' @('--preset', 'windows-msvc-core-release')
    Invoke-VerificationStep 'core release build' 'cmake' @('--build', '--preset', 'windows-msvc-core-release')
    Invoke-VerificationStep 'core release tests' 'ctest' @('--preset', 'windows-msvc-core-release')

    Write-Section 'Desktop Debug'
    if ($qtPrefix) {
        Invoke-VerificationStep 'desktop debug configure' 'cmake' @('--preset', 'windows-msvc-desktop-debug')
        Invoke-VerificationStep 'desktop debug build' 'cmake' @('--build', '--preset', 'windows-msvc-desktop-debug')
    } else {
        $results['desktop debug configure'] = 'skipped; Qt not found'
        $results['desktop debug build'] = 'skipped; Qt not found'
        Write-Warning 'Qt was not found. Skipping desktop configure/build.'
    }
} finally {
    Pop-Location
}

Write-Section 'Verification summary'
foreach ($item in $results.GetEnumerator()) {
    Write-Host ("{0}: {1}" -f $item.Key, $item.Value)
}
