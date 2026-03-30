param(
    [string]$ConfigurePreset = "debug",
    [string]$BuildPreset = "build-debug",
    [string]$TestPreset = "test-debug",
    [switch]$RunHeadlessSmoke,
    [int]$SmokeFrames = 120,
    [switch]$PauseAtEnd,
    [string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# CMake --preset reads CMakePresets.json from the current directory; always run from repo root.
Set-Location -LiteralPath $ProjectRoot

function Invoke-Step {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][scriptblock]$Body
    )

    Write-Host ""
    Write-Host "=== $Name ===" -ForegroundColor Cyan
    & $Body
}

try {
    Invoke-Step -Name "Git status snapshot" -Body {
        git status --short
    }

    Invoke-Step -Name "Configure ($ConfigurePreset)" -Body {
        cmake --preset $ConfigurePreset
    }

    Invoke-Step -Name "Build ($BuildPreset)" -Body {
        cmake --build --preset $BuildPreset
    }

    Invoke-Step -Name "Test ($TestPreset)" -Body {
        ctest --preset $TestPreset
    }

    if ($RunHeadlessSmoke) {
        $exe = Join-Path $ProjectRoot "build\vs-$ConfigurePreset\game\Debug\marbles.exe"
        if (-not (Test-Path $exe)) {
            throw "Smoke executable not found at $exe"
        }

        Invoke-Step -Name "Headless smoke run" -Body {
            & $exe --headless --frames $SmokeFrames --diag-interval 0.5
            if ($LASTEXITCODE -ne 0) {
                throw "Smoke run exited with code $LASTEXITCODE"
            }
        }
    }

    Write-Host ""
    Write-Host "Preflight complete." -ForegroundColor Green
} finally {
    if ($PauseAtEnd) {
        Read-Host "Press Enter to exit"
    }
}
