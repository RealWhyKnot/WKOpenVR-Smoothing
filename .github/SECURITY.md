# Security Policy

## Reporting a vulnerability

If you find a security issue in this project, please **don't** file a public GitHub issue. Instead use GitHub's [private vulnerability reporting](https://github.com/RealWhyKnot/FingerSmoothing/security/advisories/new) form.

I'll acknowledge within a week and aim to release a fix or workaround within 30 days. If the issue has user-impacting consequences (e.g. a malicious peer can crash SteamVR via the IPC pipe), I'll coordinate disclosure with you.

## Threat model summary

FingerSmoothing runs as:

- **An overlay app** (`FingerSmoothing.exe`) — runs at user privilege, talks only to the local SteamVR session.
- **A SteamVR driver DLL** (`driver_01fingersmoothing.dll`) — loaded into `vrserver.exe` at user privilege. Hooks `IVRDriverInput::UpdateSkeletonComponent` via MinHook to intercept Index skeletal updates before SteamVR's input subsystem stores them.

The two communicate over a local Windows named pipe (`\\.\pipe\FingerSmoothing`). User-scoped — the pipe doesn't cross a session boundary or accept network connections.

In scope:
- Memory-safety issues in the driver DLL (anything that crashes vrserver counts).
- IPC parsing bugs that let a malicious overlay corrupt driver state.
- Bone-array out-of-bounds reads in the smoothing pipeline.

Out of scope (won't be treated as security issues):
- Bugs that require an attacker to already have your user account.
- Issues in upstream dependencies (OpenVR SDK, MinHook, GLFW, ImGui, ImPlot). Report those upstream.
- "Smoothing makes my fingers feel weird" — file a normal issue.
