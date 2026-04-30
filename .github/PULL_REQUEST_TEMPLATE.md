<!-- Linked issue (optional but appreciated): Closes #__ -->

## Summary

<!-- 1-3 sentences on the *why*, not just the what. The diff already shows the what. -->

## Checklist

- [ ] `powershell -ExecutionPolicy Bypass -File build.ps1` succeeds end-to-end on this branch.
- [ ] Default smoothing state is still **off** for fresh installs (per the real-world-testing-required rule).
- [ ] If the IPC protocol changed, `protocol::Version` was bumped in `src/common/Protocol.h`.
- [ ] Commit subjects pass `.githooks/commit-msg` — no duplicate `(YYYY.M.D.N-XXXX)` build-version stamps.

## Notes for driver / hook / IPC changes

<!-- Delete this section if irrelevant. Otherwise: -->
<!-- - Did you change the skeletal-hook detour? Note any new bone-array invariants you assume. -->
<!-- - Did you add a new IPC message? Bump protocol::Version. -->
<!-- - Did you touch the smoothing math? Mention which empirical tests you ran (VRChat session, Khronos hello_xr, etc.). -->
