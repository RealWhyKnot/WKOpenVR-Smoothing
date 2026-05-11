## Install (fresh)

1. Download `{zip-name}` from this release.
2. Extract anywhere except `Program Files`.
3. With SteamVR closed, copy the `01openvrpair/` folder from the extracted zip into `<your Steam>\steamapps\common\SteamVR\drivers\`. If that folder already exists (because OpenVR-WKSpaceCalibrator is installed), the new copy overwrites the driver DLL but leaves any existing `enable_calibration.flag` in place so calibration keeps working.
4. Run `OpenVR-WKSmoothing.exe`. The settings UI opens; toggle "Enable finger smoothing" and drag the strength slider to taste.
5. Launch SteamVR. With Index Knuckles connected, finger jitter should be visibly reduced as soon as the next app sees skeletal input.

If only OpenVR-WKSmoothing is installed (no SC), the shared driver runs with calibration dormant and only the smoothing hooks active.
