# OpenVR-WKSmoothing

Finger-smoothing config UI for the [OpenVR-WKPairDriver](https://github.com/RealWhyKnot/OpenVR-WKPairDriver) shared SteamVR driver. Pairs with [OpenVR-WKSpaceCalibrator](https://github.com/RealWhyKnot/OpenVR-WKSpaceCalibrator) -- they share the same driver DLL but each toggles its own subsystem on or off via a marker flag file.

## What it does

Index Knuckles publish per-frame finger bone arrays through `IVRDriverInput::UpdateSkeletonComponent`. With smoothing enabled, OpenVR-WKPairDriver intercepts that call and applies a per-bone slerp before the bones reach OpenVR consumers. The result: jittery thumb / pinky tracking on the Knuckles produces visibly steadier hand poses in VRChat (and any other app reading skeletal input).

This overlay is the configuration GUI for that feature. It connects to the driver over a named pipe and pushes updates live as you drag the strength slider, so you can dial in the right amount of smoothing without restarting SteamVR.

The smoothing logic itself lives in OpenVR-WKPairDriver -- this repo just contains the UI.

## Install

End users: download the `OpenVR-WKSmoothing-vN.zip` from the [releases page](https://github.com/RealWhyKnot/OpenVR-WKSmoothing/releases/latest), extract anywhere, copy the `01openvrpair/` folder into `<your Steam>\steamapps\common\SteamVR\drivers\`, and run `OpenVR-WKSmoothing.exe`.

If OpenVR-WKSpaceCalibrator is also installed, it shares the same `01openvrpair/` folder. Either install order is fine; whichever installer runs second overwrites the driver DLL with whichever bundled copy is newer (per-build version gate) and adds its own `enable_*.flag`.

## Build

Requires CMake 3.15+, MSVC (Visual Studio 2022 recommended), and submodules initialised.

```
git clone --recursive https://github.com/RealWhyKnot/OpenVR-WKSmoothing
cd OpenVR-WKSmoothing
./build.ps1
```

Output: `build/artifacts/Release/OpenVR-WKSmoothing.exe`. Pass `-Release` to produce a distribution zip + manifest under `release/`.

## Pipes

`\\.\pipe\OpenVR-WKSmoothing` -- this overlay <-> driver. Wire format is `protocol::Request` / `protocol::Response` from [lib/OpenVR-WKPairDriver/src/common/Protocol.h](https://github.com/RealWhyKnot/OpenVR-WKPairDriver/blob/main/src/common/Protocol.h). Only `RequestHandshake` and `RequestSetFingerSmoothing` are accepted on this pipe; the driver's per-pipe feature mask rejects calibration requests on the smoothing pipe.

## License

GNU General Public License v3.0, see [LICENSE](LICENSE). Project copyright lines and third-party attributions in [NOTICE](NOTICE).
