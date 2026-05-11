# Changelog

Mirror of the root `CHANGELOG.md`, kept in lock-step by `.github/scripts/Update-Changelog.ps1`.

## Unreleased

### Added
- Rebuild as smoothing config UI for OpenVR-WKPairDriver (6da0497)
- **repo:** Own Smoothing driver source and overlay tab plugin (1c67a47)
- **overlay:** Rebuild smoothing plugin around shell card widgets (3d97ba4)
- **smoothing:** Take ownership of per-tracker pose prediction (v12) (14e2d0e)
- **overlay:** SC-style density pass on Prediction + Fingers sub-tabs (c17d40a)
- **overlay:** Per-finger smoothness strength (protocol v13) (7b41a87)

### Changed
- **deps:** Bump PairDriver for InputHealth range snapshots (2dd2881)
- Final diagnostic pass: full IVRDriverInput coverage, vtable disassembly, serial-number lookup, handle inventory (0a7239e)
- Round 2 review pass: MinHook lifecycle fixes + module-diff heartbeat (24c131b)
- M1 round 2: comprehensive driver-side diagnostics (6ce4414)
- Revert "feat(overlay): rebuild smoothing plugin around shell card widgets" (1018aeb)
- Rename OpenVR modules to WK variants (7b95369)
- **overlay:** Split SmoothingPlugin into Fingers + Prediction files (ed01cae)

### Fixed
- **deploy:** Verify loader-named pair driver (8c4fe7c)
- **release:** Install shared driver under loader name (dc44005)
