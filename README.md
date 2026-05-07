# OpenVR-Smoothing

Finger-smoothing config UI for the [OpenVR-PairDriver](https://github.com/RealWhyKnot/OpenVR-PairDriver) shared SteamVR driver. Pairs with [OpenVR-SpaceCalibrator](https://github.com/RealWhyKnot/OpenVR-SpaceCalibrator) -- they share the same driver DLL but each toggles its own subsystem on or off via a marker flag file.

## What it does

Index Knuckles publish per-frame finger bone arrays through `IVRDriverInput::UpdateSkeletonComponent`. With smoothing enabled, OpenVR-PairDriver intercepts that call and applies a per-bone slerp before the bones reach OpenVR consumers. The result: jittery thumb / pinky tracking on the Knuckles produces visibly steadier hand poses in VRChat (and any other app reading skeletal input).

This overlay is the configuration GUI for that feature. It connects to the driver over a named pipe and pushes updates live as you drag the strength slider, so you can dial in the right amount of smoothing without restarting SteamVR.

The smoothing logic itself lives in OpenVR-PairDriver -- this repo just contains the UI.

## Install

End users: download the `OpenVR-Smoothing-vN.zip` from the [releases page](https://github.com/RealWhyKnot/OpenVR-Smoothing/releases/latest), extract anywhere, copy the `01openvrpair/` folder into `<your Steam>\steamapps\common\SteamVR\drivers\`, and run `OpenVR-Smoothing.exe`.

If OpenVR-SpaceCalibrator is also installed, it shares the same `01openvrpair/` folder. Either install order is fine; whichever installer runs second overwrites the driver DLL with whichever bundled copy is newer (per-build version gate) and adds its own `enable_*.flag`.

## Build

Requires CMake 3.15+, MSVC (Visual Studio 2022 recommended), and submodules initialised.

```
git clone --recursive https://github.com/RealWhyKnot/OpenVR-Smoothing
cd OpenVR-Smoothing
./build.ps1
```

Output: `build/artifacts/Release/OpenVR-Smoothing.exe`. Pass `-Release` to produce a distribution zip + manifest under `release/`.

## Pipes

`\\.\pipe\OpenVR-Smoothing` -- this overlay <-> driver. Wire format is `protocol::Request` / `protocol::Response` from [lib/OpenVR-PairDriver/src/common/Protocol.h](https://github.com/RealWhyKnot/OpenVR-PairDriver/blob/main/src/common/Protocol.h). Only `RequestHandshake` and `RequestSetFingerSmoothing` are accepted on this pipe; the driver's per-pipe feature mask rejects calibration requests on the smoothing pipe.

## License

GNU General Public License v3.0, see [LICENSE](LICENSE). Project copyright lines and third-party attributions in [NOTICE](NOTICE).
