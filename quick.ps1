param(
    # Skip the cmake configure step. Default ON for quick.ps1 -- once
    # configured, MSBuild rebuilds from the existing solution. Pass
    # -Reconfigure to force a fresh cmake configure.
    [switch]$Reconfigure,

    # After building, hot-swap the runtime files into the existing install
    # at $InstallPath. Self-elevates so you don't need an admin shell first.
    [switch]$Install,

    # Full deploy: closes Steam, builds the OpenVR-WKPairDriver submodule,
    # copies the shared driver tree into <SteamVR>/drivers/01openvrpair/,
    # drops enable_smoothing.flag, hot-swaps the overlay (implies -Install),
    # then relaunches Steam. Self-elevates for the admin operations.
    [switch]$DeployDriver,

    # Where the install lives. Override only if installed somewhere
    # non-default.
    [string]$InstallPath = "C:\Program Files\OpenVR-WKSmoothing",

    # Where Steam lives. Required for -DeployDriver.
    [string]$SteamPath = "C:\Program Files (x86)\Steam",

    # Skip the interactive "are you sure?" prompt before -Install /
    # -DeployDriver shut Steam + SteamVR down.
    [switch]$Yes
)

if ($DeployDriver -and -not $Install) { $Install = $true }

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

if (($Install -or $DeployDriver) -and -not $Yes) {
    Write-Host ""
    Write-Host "About to do a hot-swap install. This will:" -ForegroundColor Yellow
    if ($DeployDriver) {
        Write-Host "  - Close Steam (incl. any running game)" -ForegroundColor Yellow
        Write-Host "  - Force-kill SteamVR helpers" -ForegroundColor Yellow
        Write-Host "  - Close the OpenVR-WKSmoothing overlay" -ForegroundColor Yellow
        Write-Host "  - Build + copy the shared driver tree into SteamVR\drivers\01openvrpair (UAC)" -ForegroundColor Yellow
        Write-Host "  - Hot-swap the overlay into $InstallPath (UAC)" -ForegroundColor Yellow
        Write-Host "  - Relaunch Steam" -ForegroundColor Yellow
    } else {
        Write-Host "  - Close the OpenVR-WKSmoothing overlay (if running)" -ForegroundColor Yellow
        Write-Host "  - Hot-swap the overlay into $InstallPath (UAC)" -ForegroundColor Yellow
    }
    Write-Host ""
    Write-Host "If you're in VR right now, this WILL kick you out." -ForegroundColor Yellow
    $resp = Read-Host "Proceed? [y/N]"
    if ($resp -notmatch '^[yY]') {
        Write-Host "Aborted by user. Re-run with -Yes to skip this prompt." -ForegroundColor DarkGray
        exit 0
    }
}

# Hashtable splat: switches bind by name (array splat would feed switch
# strings into build.ps1's first positional parameter $Version).
$buildArgs = @{}
if (-not $Reconfigure) { $buildArgs.SkipConfigure = $true }

Write-Host "quick.ps1: forwarding to build.ps1 $((($buildArgs.GetEnumerator() | ForEach-Object { '-' + $_.Name }) -join ' '))" -ForegroundColor DarkGray
& (Join-Path $PSScriptRoot "build.ps1") @buildArgs
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) {
    throw "build.ps1 failed (exit $exitCode)"
}

$ArtifactsDir = Join-Path $PSScriptRoot "build/artifacts/Release"
$Overlay      = Join-Path $ArtifactsDir "OpenVR-WKSmoothing.exe"
if (-not (Test-Path $Overlay)) {
    throw "Build said it succeeded but $Overlay isn't there."
}

if (-not $Install) {
    Write-Host ""
    Write-Host "Built overlay:  $Overlay" -ForegroundColor Green
    Write-Host ""
    Write-Host "Run directly from the build dir:" -ForegroundColor DarkGray
    Write-Host "  & '$Overlay'" -ForegroundColor DarkGray
    Write-Host ""
    Write-Host "Or build + hot-swap into your installed copy in one step:" -ForegroundColor DarkGray
    Write-Host "  ./quick.ps1 -Install            (overlay only)" -ForegroundColor DarkGray
    Write-Host "  ./quick.ps1 -DeployDriver       (overlay + driver; closes Steam)" -ForegroundColor DarkGray
    Write-Host "  ./quick.ps1 -DeployDriver -Yes  (skip y/N prompt)" -ForegroundColor DarkGray
    return
}

# Driver-deploy paths
$PairDriverRoot   = Join-Path $PSScriptRoot "lib/OpenVR-WKPairDriver"
$PairDriverTree   = Join-Path $PairDriverRoot "build/driver_openvrpair"
$PairDriverDll    = Join-Path $PairDriverTree "bin/win64/driver_openvrpair.dll"
$SteamExe         = Join-Path $SteamPath "steam.exe"
$SteamDriversDir  = Join-Path $SteamPath "steamapps/common/SteamVR/drivers"
$DestDriverFolder = Join-Path $SteamDriversDir "01openvrpair"
$DestDriverDll    = Join-Path $DestDriverFolder "bin/win64/driver_01openvrpair.dll"
$BareDestDll      = Join-Path $DestDriverFolder "bin/win64/driver_openvrpair.dll"
$DestResourcesDir = Join-Path $DestDriverFolder "resources"
$DestSmFlag       = Join-Path $DestResourcesDir "enable_smoothing.flag"
# Legacy driver folder this overlay's predecessor installed under. Migration
# disables it by renaming its manifest; the folder itself stays in place
# (rollback = rename-the-manifest-back).
$LegacyFsManifest = Join-Path $SteamDriversDir "01fingersmoothing/driver.vrdrivermanifest"

if ($DeployDriver) {
    if (-not (Test-Path $PairDriverRoot)) {
        throw "OpenVR-WKPairDriver submodule not found at '$PairDriverRoot'. Run 'git submodule update --init --recursive'."
    }
    Write-Host ""
    Write-Host "--- Building OpenVR-WKPairDriver submodule ---" -ForegroundColor Cyan
    Push-Location $PairDriverRoot
    try {
        & (Join-Path $PairDriverRoot "build.ps1")
        if ($LASTEXITCODE -ne 0) { throw "Submodule build failed (exit $LASTEXITCODE)" }
    } finally {
        Pop-Location
    }
    if (-not (Test-Path $PairDriverDll)) {
        throw "Submodule built but driver DLL not at '$PairDriverDll'."
    }
    if (-not (Test-Path $SteamExe)) {
        throw "Steam not found at '$SteamExe'."
    }
    if (-not (Test-Path $SteamDriversDir)) {
        throw "SteamVR drivers folder not found at '$SteamDriversDir'."
    }

    $steamRunning = @(Get-Process -Name steam -ErrorAction SilentlyContinue).Count -gt 0
    if ($steamRunning) {
        Write-Host ""
        Write-Host "Sending Steam graceful shutdown..." -ForegroundColor Green
        & $SteamExe -shutdown | Out-Null
        $deadline = (Get-Date).AddSeconds(30)
        while ((Get-Process -Name steam -ErrorAction SilentlyContinue) -and (Get-Date) -lt $deadline) {
            Start-Sleep -Milliseconds 500
        }
        if (Get-Process -Name steam -ErrorAction SilentlyContinue) {
            Write-Host "Steam still up after 30s; force-killing Steam + SteamVR processes..." -ForegroundColor Yellow
            Stop-Process -Name steam,steamwebhelper,vrserver,vrmonitor,vrwebhelper,vrcompositor,vrstartup -Force -ErrorAction SilentlyContinue
            Start-Sleep -Seconds 2
        }
        Write-Host "Steam closed." -ForegroundColor Green
    } else {
        Write-Host "Steam not running; skipping shutdown." -ForegroundColor DarkGray
    }
    Stop-Process -Name vrserver,vrmonitor,vrwebhelper,vrcompositor,vrstartup,vrdashboard -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500
}

if (-not (Test-Path $InstallPath)) {
    throw "Install path '$InstallPath' doesn't exist. Either install via the NSIS installer first, or pass -InstallPath '<your path>'."
}

$RuntimeFiles = @("OpenVR-WKSmoothing.exe")
$copyCmds = $RuntimeFiles | ForEach-Object {
    "Copy-Item -Force -Path '$ArtifactsDir\$_' -Destination '$InstallPath\$_'"
}

$wasRunning = @(Get-Process -Name OpenVR-WKSmoothing -ErrorAction SilentlyContinue).Count -gt 0

# When -DeployDriver is on, the elevated block also installs the shared
# driver and drops enable_smoothing.flag. Pre-existing 01openvrpair gets
# replaced; legacy 01fingersmoothing gets disabled by manifest rename.
$extraCmds = @()
if ($DeployDriver) {
    # Preserve enable_calibration.flag if SC has already dropped it -- the
    # shared driver's two consumers each own their own flag and replacing
    # the folder wholesale would clobber SC's flag and turn off calibration
    # until the next SC install.
    $existingCalFlag = "$DestResourcesDir\enable_calibration.flag"
    $extraCmds += "if (Test-Path '$DestDriverFolder') { Remove-Item -Recurse -Force '$DestDriverFolder' }"
    $extraCmds += "Copy-Item -Recurse -Force -Path '$PairDriverTree' -Destination '$DestDriverFolder'"
    $extraCmds += "if (Test-Path '$BareDestDll') { Move-Item -Force -Path '$BareDestDll' -Destination '$DestDriverDll' }"
    $extraCmds += "if (-not (Test-Path '$DestResourcesDir')) { New-Item -ItemType Directory -Force -Path '$DestResourcesDir' | Out-Null }"
    $extraCmds += "Set-Content -Path '$DestSmFlag' -Value 'enabled' -NoNewline"
    $extraCmds += "Write-Host 'Installed shared driver tree to $DestDriverFolder + dropped enable_smoothing.flag.'"
    $disabledFsManifest = "$LegacyFsManifest.disabled-by-pair-migration"
    $extraCmds += "if (Test-Path '$LegacyFsManifest') { Move-Item -Force -Path '$LegacyFsManifest' -Destination '$disabledFsManifest'; Write-Host 'Disabled legacy 01fingersmoothing driver (renamed manifest).' }"
}

$elevatedScript = @"
Stop-Process -Name OpenVR-WKSmoothing -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 500
$($copyCmds -join "`n")
$($extraCmds -join "`n")
Write-Host 'Done. Window will close in 2 seconds.'
Start-Sleep -Seconds 2
"@

Write-Host ""
Write-Host "Hot-swapping into: $InstallPath" -ForegroundColor Green
Start-Process powershell -Verb RunAs -ArgumentList "-NoProfile","-Command",$elevatedScript -Wait

if ($DeployDriver) {
    if (Test-Path $DestDriverDll) {
        $driverBuildTime     = (Get-Item $PairDriverDll).LastWriteTime
        $driverInstalledTime = (Get-Item $DestDriverDll).LastWriteTime
        if ($driverInstalledTime -ge $driverBuildTime.AddSeconds(-2)) {
            Write-Host "Driver DLL timestamps match build." -ForegroundColor Green
        } else {
            Write-Host "WARNING: installed driver DLL older than the build." -ForegroundColor Yellow
        }
    }
    if (Test-Path $DestSmFlag) {
        Write-Host "enable_smoothing.flag present at $DestSmFlag." -ForegroundColor Green
    } else {
        Write-Host "WARNING: enable_smoothing.flag missing; the driver will run inert for smoothing on next launch." -ForegroundColor Yellow
    }

    Write-Host ""
    Write-Host "Relaunching Steam..." -ForegroundColor Green
    Start-Process -FilePath $SteamExe
}
