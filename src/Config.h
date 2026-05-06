#pragma once

#include <cstdint>

// Persisted finger-smoothing UI state. Saved to
// %LocalAppDataLow%\OpenVR-Smoothing\config.txt as plain key=value lines so
// it's trivially editable by hand if needed.
struct SmoothingConfig
{
    bool     master_enabled = false;
    int      smoothness     = 50;          // 0..100
    uint16_t finger_mask    = 0x03FF;      // protocol::kAllFingersMask -- all 10 fingers
};

// Load from disk. On any read / parse error the on-disk file is ignored and
// a default-constructed SmoothingConfig is returned (so first launch and
// corrupt-file launch both produce the same sensible defaults).
SmoothingConfig LoadConfig();

// Save to disk. Best-effort: failures (locked file, missing dir) are
// silently swallowed -- the next save will retry. The driver gets the live
// value via IPC regardless of persistence success.
void SaveConfig(const SmoothingConfig &cfg);
