param(
    # Skip the cmake configure step. Default ON for quick.ps1 — if you've already
    # configured once, MSBuild can rebuild from the existing solution. Pass
    # -Reconfigure to force a fresh cmake configure.
    [switch]$Reconfigure,

    # By default we skip both configure AND release-zip packaging so the
    # iteration loop is just "save -> run quick.ps1 -> launch". Pass -Zip to
    # also produce the drop-in distribution zip.
    [switch]$Zip,

    # After building, hot-swap the runtime files into the existing install at
    # $InstallPath. Self-elevates so you don't need to spawn an admin shell
    # first. Without -Install, quick.ps1 just builds.
    #
    # Note: -Install only swaps the OVERLAY exe + assets. Replacing the
    # installed driver DLL requires SteamVR to be closed (vrserver.exe holds
    # the file lock). Do that step manually after confirming you're not in a
    # session.
    [switch]$Install,

    # Where the install lives. Override only if you've installed somewhere
    # non-default.
    [string]$InstallPath = "C:\Program Files\FingerSmoothing"
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$buildArgs = @()
if (-not $Reconfigure) { $buildArgs += "-SkipConfigure" }
if (-not $Zip)         { $buildArgs += "-SkipZip" }

Write-Host "quick.ps1: forwarding to build.ps1 $($buildArgs -join ' ')" -ForegroundColor DarkGray
& powershell.exe -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "build.ps1") @buildArgs
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) {
    throw "build.ps1 failed (exit $exitCode)"
}

$ArtifactsDir = Join-Path $PSScriptRoot "bin/artifacts/Release"
$Overlay      = Join-Path $ArtifactsDir "FingerSmoothing.exe"
if (-not (Test-Path $Overlay)) {
    throw "Build said it succeeded but $Overlay isn't there. Something is off."
}

if (-not $Install) {
    Write-Host ""
    Write-Host "Built overlay:  $Overlay" -ForegroundColor Green
    Write-Host ""
    Write-Host "Run directly from the build dir:" -ForegroundColor DarkGray
    Write-Host "  & '$Overlay'" -ForegroundColor DarkGray
    Write-Host ""
    Write-Host "Or build + hot-swap into your installed copy in one step:" -ForegroundColor DarkGray
    Write-Host "  ./quick.ps1 -Install" -ForegroundColor DarkGray
    return
}

# --- Hot-swap install -----------------------------------------------------

if (-not (Test-Path $InstallPath)) {
    throw "Install path '$InstallPath' doesn't exist. Either install via the NSIS installer first, or pass -InstallPath '<your path>'."
}

$RuntimeFiles = @("FingerSmoothing.exe", "icon.png", "taskbar_icon.png", "manifest.vrmanifest", "openvr_api.dll")

$copyCmds = $RuntimeFiles | ForEach-Object {
    "Copy-Item -Force -Path '$ArtifactsDir\$_' -Destination '$InstallPath\$_'"
}

$wasRunning = @(Get-Process -Name FingerSmoothing -ErrorAction SilentlyContinue).Count -gt 0
$installedExe = Join-Path $InstallPath "FingerSmoothing.exe"

$elevatedScript = @"
Stop-Process -Name FingerSmoothing -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 500
$($copyCmds -join "`n")
Write-Host 'Files copied. Refreshing icon cache...'
& ie4uinit.exe -show 2>&1 | Out-Null
Write-Host 'Done. Window will close in 2 seconds.'
Start-Sleep -Seconds 2
"@

Write-Host ""
Write-Host "Hot-swapping into: $InstallPath" -ForegroundColor Green
if ($wasRunning) {
    Write-Host "Detected running instance -- will re-launch after install." -ForegroundColor DarkGray
}
Write-Host "Approve the UAC prompt when it appears." -ForegroundColor DarkGray
Start-Process powershell -Verb RunAs -ArgumentList "-NoProfile","-Command",$elevatedScript -Wait

if ($wasRunning -and (Test-Path $installedExe)) {
    Write-Host "Re-launching $installedExe ..." -ForegroundColor DarkGray
    Start-Process -FilePath $installedExe -WorkingDirectory $InstallPath
}

Write-Host ""
$buildTime = (Get-Item $Overlay).LastWriteTime
if (Test-Path $installedExe) {
    $installedTime = (Get-Item $installedExe).LastWriteTime
    if ($installedTime -ge $buildTime.AddSeconds(-2)) {
        Write-Host "Install timestamps match build. Launch from Start menu / pinned shortcut." -ForegroundColor Green
    } else {
        Write-Host "WARNING: installed EXE older than the build. Did UAC get cancelled?" -ForegroundColor Yellow
        Write-Host "  build:     $buildTime"
        Write-Host "  installed: $installedTime"
    }
}
