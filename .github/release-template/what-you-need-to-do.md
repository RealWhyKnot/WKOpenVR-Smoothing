## What you need to do

If you are already on a prior release and OpenVR-WKSpaceCalibrator is also installed: just download the new zip and copy the `01openvrpair/` folder over the existing one. The shared driver's per-build version stamp keeps the newer DLL; SC's `enable_calibration.flag` is preserved as long as you don't delete the resources/ folder first. SC's overlay does not need to restart.

If only OpenVR-WKSmoothing is installed: same procedure, replace `01openvrpair/` with the new copy.

If something looks off after upgrading: close `OpenVR-WKSmoothing.exe`, restart SteamVR (so the driver re-reads the flag files and re-installs hooks against the new DLL), then reopen the overlay. Settings persist in `%LocalAppDataLow%\OpenVR-WKSmoothing\config.txt` and reload on launch.
