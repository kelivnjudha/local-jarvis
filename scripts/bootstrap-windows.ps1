[CmdletBinding()]
param(
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [switch]$Yes
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$summary = [ordered]@{
    Git = 'not checked'
    CMake = 'not checked'
    MSVC = 'not checked'
    Vcpkg = 'not checked'
    Qt = 'not checked'
    CoreDebug = 'not run'
    DesktopDebug = 'not run'
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

function Test-CommandAvailable {
    param([string]$Name)
    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
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

function Find-Msvc {
    if (Test-CommandAvailable 'cl') {
        return 'cl on PATH'
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (!(Test-Path $vswhere)) {
        $command = Get-Command 'vswhere' -ErrorAction SilentlyContinue
        if ($command) {
            $vswhere = $command.Source
        }
    }

    if (Test-Path $vswhere) {
        $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($LASTEXITCODE -eq 0 -and ![string]::IsNullOrWhiteSpace($installPath)) {
            return "Visual Studio C++ tools at $installPath"
        }
    }

    return $null
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

    $commonRoots = @('C:\Qt')
    foreach ($root in $commonRoots) {
        if (!(Test-Path $root)) {
            continue
        }

        $candidate = Get-ChildItem -Path $root -Directory -ErrorAction SilentlyContinue |
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

Write-Section 'Toolchain checks'

if (!(Test-CommandAvailable 'git')) {
    throw 'Git was not found on PATH. Install Git for Windows.'
}
$summary.Git = 'found'
Write-Host 'Git: found'

if (!(Test-CommandAvailable 'cmake')) {
    throw 'CMake was not found on PATH. Install CMake 3.22 or newer.'
}
$summary.CMake = (& cmake --version | Select-Object -First 1)
Write-Host "CMake: $($summary.CMake)"

$msvc = Find-Msvc
if (!$msvc) {
    throw 'MSVC C++ tools were not found. Install Visual Studio 2022 Build Tools with Desktop development with C++.'
}
$summary.MSVC = $msvc
Write-Host "MSVC: $msvc"

Write-Section 'vcpkg setup'

if ([string]::IsNullOrWhiteSpace($VcpkgRoot)) {
    $VcpkgRoot = 'C:\dev\vcpkg'
    Write-Warning "VCPKG_ROOT is not set. Suggested default: $VcpkgRoot"
}

$env:VCPKG_ROOT = $VcpkgRoot
$env:VCPKG_DISABLE_METRICS = '1'

if (!(Test-Path $env:VCPKG_ROOT)) {
    if (Confirm-Step "Clone vcpkg to $env:VCPKG_ROOT?") {
        New-Item -ItemType Directory -Force -Path (Split-Path $env:VCPKG_ROOT -Parent) | Out-Null
        Invoke-Checked 'vcpkg clone' 'git' @('clone', 'https://github.com/microsoft/vcpkg.git', $env:VCPKG_ROOT)
    } else {
        throw "vcpkg directory does not exist: $env:VCPKG_ROOT"
    }
}

$bootstrapScript = Join-Path $env:VCPKG_ROOT 'bootstrap-vcpkg.bat'
$vcpkgExe = Join-Path $env:VCPKG_ROOT 'vcpkg.exe'
if (!(Test-Path $bootstrapScript)) {
    throw "vcpkg bootstrap script was not found: $bootstrapScript"
}

if (!(Test-Path $vcpkgExe)) {
    Invoke-Checked 'vcpkg bootstrap' $bootstrapScript @()
}

$summary.Vcpkg = $env:VCPKG_ROOT
Write-Host "vcpkg: $env:VCPKG_ROOT"

Write-Section 'Qt discovery'

$qtPrefix = Find-QtPrefix
if ($qtPrefix) {
    $env:LOCAL_JARVIS_QT_PREFIX = $qtPrefix
    $summary.Qt = $qtPrefix
    Write-Host "Qt: $qtPrefix"
} else {
    $summary.Qt = 'not found; desktop build will be skipped'
    Write-Warning 'Qt was not found. Install Qt 6 MSVC 64-bit and set LOCAL_JARVIS_QT_PREFIX or CMAKE_PREFIX_PATH.'
    Write-Host 'Example: $env:LOCAL_JARVIS_QT_PREFIX = "C:\Qt\<version>\msvc2019_64"'
}

Write-Section 'Core Debug verification'
Push-Location $repoRoot
try {
    Invoke-Checked 'core debug configure' 'cmake' @('--preset', 'windows-msvc-core-debug')
    Invoke-Checked 'core debug build' 'cmake' @('--build', '--preset', 'windows-msvc-core-debug')
    Invoke-Checked 'core debug tests' 'ctest' @('--preset', 'windows-msvc-core-debug')
    $summary.CoreDebug = 'passed'

    if ($qtPrefix) {
        Write-Section 'Desktop Debug verification'
        Invoke-Checked 'desktop debug configure' 'cmake' @('--preset', 'windows-msvc-desktop-debug')
        Invoke-Checked 'desktop debug build' 'cmake' @('--build', '--preset', 'windows-msvc-desktop-debug')
        $summary.DesktopDebug = 'passed'
    } else {
        $summary.DesktopDebug = 'skipped; Qt not found'
    }
} finally {
    Pop-Location
}

Write-Section 'Final status'
foreach ($item in $summary.GetEnumerator()) {
    Write-Host ("{0}: {1}" -f $item.Key, $item.Value)
}
