#pragma once

#include "Protocol.h"

#include <string>

namespace fs_config
{
    // Load the persisted user config from %APPDATA%\FingerSmoothing\config.json.
    // Returns the on-disk config if the file exists and parses cleanly; otherwise
    // returns sane defaults (master OFF, paper One-Euro params, all fingers in
    // mask, adaptive off). Default-OFF is per `feedback_real_world_testing_required.md` —
    // smoothing is opt-in, never auto-on for a fresh install.
    protocol::SmoothingConfig Load();

    // Persist the current config. Best-effort: failures are logged but not
    // surfaced as exceptions; the running config is still authoritative even if
    // disk I/O fails (e.g. AppData is read-only on a locked-down corporate box).
    void Save(const protocol::SmoothingConfig &cfg);

    // Resolve %APPDATA%\FingerSmoothing — the directory the config and any
    // future log files live in. Created if missing.
    std::string AppDataDir();
}
