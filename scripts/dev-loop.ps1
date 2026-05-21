param(
    [string]$Preset = "debug",
    [string]$BuildPreset = "build-debug",
    [string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# CMake --preset reads CMakePresets.json from the current directory; always run from repo root.
Set-Location -LiteralPath $ProjectRoot

$sourceDirs = @(
    (Join-Path $ProjectRoot "engine"),
    (Join-Path $ProjectRoot "game"),
    (Join-Path $ProjectRoot "tests")
)

$filters = @("*.cpp", "*.hpp", "*.h", "CMakeLists.txt")
$changed = $false
$needsRestart = $false
$appProcess = $null

function Configure-And-Build {
    Write-Host "Configuring preset '$Preset'..."
    & cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { return $false }

    Write-Host "Building preset '$BuildPreset'..."
    & cmake --build --preset $BuildPreset
    return ($LASTEXITCODE -eq 0)
}

function Start-App {
    $exePath = Join-Path $ProjectRoot "build\vs-$Preset\game\$Preset\garden.exe"
    if (-not (Test-Path $exePath)) {
        Write-Host "Executable not found at $exePath"
        return $null
    }

    Write-Host "Launching $exePath"
    return Start-Process -FilePath $exePath -PassThru
}

function Stop-App {
    param([System.Diagnostics.Process]$Process)
    if ($null -eq $Process) { return }
    if (-not $Process.HasExited) {
        Stop-Process -Id $Process.Id -Force
    }
}

$watchers = @()
foreach ($dir in $sourceDirs) {
    if (-not (Test-Path $dir)) { continue }
    foreach ($filter in $filters) {
        $watcher = New-Object System.IO.FileSystemWatcher
        $watcher.Path = $dir
        $watcher.Filter = $filter
        $watcher.IncludeSubdirectories = $true
        $watcher.NotifyFilter = [IO.NotifyFilters]'FileName, LastWrite, Size, CreationTime'
        $watcher.EnableRaisingEvents = $true
        $watchers += $watcher

        Register-ObjectEvent -InputObject $watcher -EventName Changed -Action { $script:changed = $true } | Out-Null
        Register-ObjectEvent -InputObject $watcher -EventName Created -Action { $script:changed = $true } | Out-Null
        Register-ObjectEvent -InputObject $watcher -EventName Deleted -Action { $script:changed = $true } | Out-Null
        Register-ObjectEvent -InputObject $watcher -EventName Renamed -Action { $script:changed = $true } | Out-Null
    }
}

if (Configure-And-Build) {
    $appProcess = Start-App
} else {
    Write-Host "Initial build failed. Watching for changes..."
}

Write-Host "Live loop active. Press Ctrl+C to stop."
try {
    while ($true) {
        Start-Sleep -Milliseconds 400
        if (-not $changed) { continue }

        $changed = $false
        if (Configure-And-Build) {
            $needsRestart = $true
        } else {
            Write-Host "Build failed. Waiting for next change..."
            continue
        }

        if ($needsRestart) {
            Stop-App -Process $appProcess
            $appProcess = Start-App
            $needsRestart = $false
        }
    }
} finally {
    Stop-App -Process $appProcess
    Get-EventSubscriber | Unregister-Event
    foreach ($watcher in $watchers) {
        $watcher.Dispose()
    }
}
