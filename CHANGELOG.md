# Changelog

All notable user-visible changes to OpenVR-WKSmoothing. The "Unreleased" section is auto-appended by `.github/workflows/changelog-append.yml` from conventional-commit subjects on `main`; tagged sections are promoted by `.github/workflows/release.yml` on a `v*` tag push.

The release body for each tag is composed mechanically from the slice between the prior tag and the new tag plus the templated sections under `.github/release-template/`. Hand-writing release bodies is not part of the workflow.

## Unreleased

### Added
- Rebuild as smoothing config UI for OpenVR-WKPairDriver (6da0497)
- **repo:** Own Smoothing driver source and overlay tab plugin (1c67a47)
- **overlay:** Rebuild smoothing plugin around shell card widgets (3d97ba4)
- **smoothing:** Take ownership of per-tracker pose prediction (v12) (14e2d0e)
- **overlay:** SC-style density pass on Prediction + Fingers sub-tabs (c17d40a)

### Changed
- **deps:** Bump PairDriver for InputHealth range snapshots (2dd2881)
- Final diagnostic pass: full IVRDriverInput coverage, vtable disassembly, serial-number lookup, handle inventory (0a7239e)
- Round 2 review pass: MinHook lifecycle fixes + module-diff heartbeat (24c131b)
- M1 round 2: comprehensive driver-side diagnostics (6ce4414)
- Revert "feat(overlay): rebuild smoothing plugin around shell card widgets" (1018aeb)
- Rename OpenVR modules to WK variants (7b95369)

### Fixed
- **deploy:** Verify loader-named pair driver (8c4fe7c)
- **release:** Install shared driver under loader name (dc44005)
