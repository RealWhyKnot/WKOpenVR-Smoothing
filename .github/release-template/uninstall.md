## Uninstall

1. Close `OpenVR-Smoothing.exe` and SteamVR.
2. Delete `<your Steam>\steamapps\common\SteamVR\drivers\01openvrpair\resources\enable_smoothing.flag`. With this flag absent, the shared driver's smoothing hooks no longer install on the next SteamVR startup, regardless of whether the rest of the driver tree is still on disk.
3. If OpenVR-SpaceCalibrator is also installed, leave the rest of `01openvrpair/` alone. SC owns the matching `enable_calibration.flag` and keeps calibration working.
4. If you don't have SC installed and want the shared driver gone too, delete the entire `01openvrpair/` folder.
5. Optionally delete `%LocalAppDataLow%\OpenVR-Smoothing\` to remove the saved settings.
