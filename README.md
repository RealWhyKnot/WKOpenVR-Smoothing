# FingerSmoothing

A SteamVR driver that smooths Valve Index controller finger-tracking jitter. Stationary fingers stop twitching; rapid gestures still pass through. Works with any OpenVR app that consumes Index skeletal input — including VRChat.

> Status: pre-release. The driver hook is in place; smoothing math and UI are landing in active development. Real-world VRChat testing is the only validator. Default install ships with smoothing **OFF** — opt in once you've confirmed the pieces work on your setup.

## Compatibility

**In scope** — apps where this driver actively smooths fingers:

- VRChat with Index controllers via SteamVR (the primary target).
- Any OpenVR app that reads Index skeletal data via `IVRInput::GetSkeletalBoneData` / `GetSkeletalSummaryData`.

**Out of scope** — the driver does not affect these:

- Native-OpenXR apps that read finger data via `xrLocateHandJointsEXT`. The interception lives on the OpenVR skeletal-input path; the OpenXR path is separate.
- Non-Index controllers. Other controllers have no curl/splay sensors to smooth.

## How it works

Loads as a SteamVR driver into `vrserver.exe` and hooks `IVRDriverInput::UpdateSkeletonComponent` via MinHook. When Valve's Index driver publishes a skeletal update, our hook intercepts the bone array, derives the 9 summary scalars (5 curls + 4 splays), runs each through a One-Euro filter (Casiez et al., 2012), reconstructs bones from the smoothed scalars, and forwards them to SteamVR's input system. AC-safe — the modified process is `vrserver.exe`, not the game.

## Build

Windows-only. Visual Studio 2022 + CMake.

```powershell
git clone https://github.com/RealWhyKnot/FingerSmoothing.git
cd FingerSmoothing
git submodule update --init --recursive
./build.ps1
```

`./quick.ps1` for fast incremental rebuilds during iteration. `./quick.ps1 -Install` hot-swaps the overlay into the installed copy (driver swap requires SteamVR closed first — vrserver locks the DLL).

## License

MIT. See `LICENSE` for full attribution including OpenVR-SpaceCalibrator (driver/overlay scaffolding) and the third-party libraries this build links against.
