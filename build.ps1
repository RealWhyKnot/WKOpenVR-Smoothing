param(
    # Override the auto-derived YYYY.M.D.N-XXXX dev stamp. Release CI passes
    # the git tag (with leading "v" stripped) so the published release's tag,
    # zip filename, and embedded version are all the same string.
    [string]$Version = "",

    # Skip the cmake configure step.
    [switch]$SkipConfigure,

    # Produce a release zip + per-file manifest TSV under release/. Required
    # by .github/workflows/release.yml.
    [switch]$Release
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

function Clear-StaleCMakeGeneratorInstance {
    param([Parameter(Mandatory=$true)][string]$BuildDir)

    $cachePath = Join-Path $BuildDir "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cachePath)) { return }

    $instancePath = $null
    foreach ($line in Get-Content -LiteralPath $cachePath) {
        if ($line -match '^CMAKE_GENERATOR_INSTANCE:[^=]*=(.*)$') {
            $instancePath = $Matches[1]
            break
        }
    }
    if (-not $instancePath) { return }

    if (-not (Test-Path -LiteralPath $instancePath)) {
        Write-Host "CMake cached Visual Studio instance no longer exists: $instancePath" -ForegroundColor Yellow
        Write-Host "Clearing generated CMake configure cache under $BuildDir" -ForegroundColor Yellow
        Remove-Item -LiteralPath $cachePath -Force
        $cmakeFiles = Join-Path $BuildDir "CMakeFiles"
        if (Test-Path -LiteralPath $cmakeFiles) {
            Remove-Item -LiteralPath $cmakeFiles -Recurse -Force
        }
    }
}

# Activate the repo's tracked git hooks the first time the build runs in a
# clone. Idempotent: only writes when the value would change.
$currentHooksPath = & git config --get core.hooksPath 2>$null
if ($currentHooksPath -ne ".githooks") {
    & git config core.hooksPath ".githooks"
    Write-Host "Activated .githooks/ via core.hooksPath"
}

# Stamp the build version. Release CI passes -Version; local builds derive
# the stamp from today's date + a per-day counter + a 4-hex GUID prefix.
if ($Version -eq "") {
    $today = Get-Date -Format "yyyy.M.d"
    $counterFile = "build/local_build_state.json"
    $counter = 0
    if (Test-Path $counterFile) {
        $state = Get-Content $counterFile -Raw | ConvertFrom-Json
        if ($state.date -eq $today) {
            $counter = [int]$state.counter + 1
        }
    }
    $uid = ([guid]::NewGuid().ToString("N").Substring(0, 4)).ToUpper()
    $Version = "$today.$counter-$uid"
    New-Item -ItemType Directory -Force -Path "build" | Out-Null
    @{ date = $today; counter = $counter } | ConvertTo-Json | Set-Content $counterFile
}
Set-Content -Path "version.txt" -Value $Version -NoNewline
Write-Host "Build version: $Version"

# Configure (skippable for incremental edits). The CMAKE_POLICY_VERSION_MINIMUM
# bump keeps any pre-3.5 cmake_minimum_required in submodule history accepted
# by current CMake.
if (-not $SkipConfigure) {
    Clear-StaleCMakeGeneratorInstance -BuildDir "build"
    & cmake -S . -B build -A x64 "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
}

# Build Release.
& cmake --build build --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

# Verify the feature artifacts land where we expect. The Smoothing repo now
# primarily builds feature modules consumed by OpenVR-WKPairDriver; the old
# standalone exe target is not part of the normal CMake graph.
$artifactPaths = @(
    "build/artifacts/Release/openvr_pair_feature_smoothing_driver.lib",
    "build/artifacts/Release/openvr_pair_feature_smoothing_overlay.lib"
)
Write-Host ""
foreach ($artifactPath in $artifactPaths) {
    if (-not (Test-Path $artifactPath)) {
        throw "Expected feature artifact not found at $artifactPath"
    }
    $artifact = Get-Item $artifactPath
    Write-Host ("Built {0} ({1:N0} bytes, {2})" -f $artifact.Name, $artifact.Length, $artifact.LastWriteTime)
    Write-Host ("  -> {0}" -f $artifact.FullName)
}

if ($Release) {
    $exePath = "build/artifacts/Release/OpenVR-Smoothing.exe"
    if (-not (Test-Path $exePath)) {
        throw "Release packaging requires standalone exe not found at $exePath"
    }

    # Build the OpenVR-WKPairDriver submodule so its driver tree is available
    # to bundle into the release zip.
    $PairDriverRoot = Join-Path $PSScriptRoot "lib/OpenVR-WKPairDriver"
    $PairDriverTree = Join-Path $PairDriverRoot "build/driver_openvrpair"
    Write-Host ""
    Write-Host "--- Building OpenVR-WKPairDriver submodule ---" -ForegroundColor Cyan
    Push-Location $PairDriverRoot
    try {
        & (Join-Path $PairDriverRoot "build.ps1") -Version $Version
        if ($LASTEXITCODE -ne 0) { throw "Submodule build failed (exit $LASTEXITCODE)" }
    } finally {
        Pop-Location
    }
    if (-not (Test-Path $PairDriverTree)) {
        throw "Submodule built but driver tree not at $PairDriverTree"
    }

    New-Item -ItemType Directory -Force -Path "release" | Out-Null

    # Stage the release tree:
    #   <stage>/OpenVR-WKSmoothing.exe
    #   <stage>/01openvrpair/...                 (driver tree)
    #   <stage>/01openvrpair/resources/enable_smoothing.flag
    #   <stage>/version.txt
    $StageDir = Join-Path "release" "_stage_$Version"
    if (Test-Path $StageDir) { Remove-Item -Recurse -Force $StageDir }
    New-Item -ItemType Directory -Force -Path $StageDir | Out-Null

    Copy-Item -Path $exePath -Destination $StageDir
    $StagedDriverDir = Join-Path $StageDir "01openvrpair"
    Copy-Item -Recurse -Path $PairDriverTree -Destination $StagedDriverDir
    $StagedDriverBin = Join-Path $StagedDriverDir "bin/win64"
    $BareDriverDll = Join-Path $StagedDriverBin "driver_openvrpair.dll"
    $LoaderDriverDll = Join-Path $StagedDriverBin "driver_01openvrpair.dll"
    if (Test-Path $BareDriverDll) {
        Move-Item -Force -Path $BareDriverDll -Destination $LoaderDriverDll
    }
    if (-not (Test-Path $LoaderDriverDll)) {
        throw "Staged shared driver DLL not found at $LoaderDriverDll"
    }
    $StagedFlagDir = Join-Path $StagedDriverDir "resources"
    if (-not (Test-Path $StagedFlagDir)) { New-Item -ItemType Directory -Force -Path $StagedFlagDir | Out-Null }
    Set-Content -Path (Join-Path $StagedFlagDir "enable_smoothing.flag") -Value 'enabled' -NoNewline
    $Version | Set-Content -Path (Join-Path $StageDir "version.txt") -Encoding UTF8 -NoNewline

    $zipName = "OpenVR-WKSmoothing-v$Version.zip"
    $zipPath = Join-Path "release" $zipName
    if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
    Compress-Archive -Path (Join-Path $StageDir "*") -DestinationPath $zipPath -CompressionLevel Optimal
    $zipItem = Get-Item $zipPath

    # Per-file SHA256 manifest (no BOM) for the File integrity table.
    $manifestName = "OpenVR-WKSmoothing-v$Version.manifest.tsv"
    $manifestPath = Join-Path "release" $manifestName
    $rootLength = (Resolve-Path $StageDir).Path.Length + 1
    $rows = Get-ChildItem $StageDir -Recurse -File | ForEach-Object {
        $rel = $_.FullName.Substring($rootLength).Replace('\', '/')
        $h = (Get-FileHash $_.FullName -Algorithm SHA256).Hash
        "{0}`t{1}`t{2}" -f $h, $_.Length, $rel
    }
    $enc = [System.Text.UTF8Encoding]::new($false)
    [System.IO.File]::WriteAllLines((Resolve-Path -LiteralPath (Split-Path $manifestPath)).Path + "\" + (Split-Path -Leaf $manifestPath), $rows, $enc)

    Remove-Item -Recurse -Force $StageDir

    Write-Host ""
    Write-Host ("Packaged release zip:      {0} ({1:N0} bytes)" -f $zipItem.Name, $zipItem.Length)
    Write-Host ("Packaged release manifest: {0}" -f $manifestName)
}
