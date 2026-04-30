#pragma once

#include "Protocol.h"
#include "IPCClient.h"

namespace fs_ui
{
    // Render one frame of the UI. Mutates `cfg` in place when the user touches
    // a control; the caller is responsible for noticing the change, saving it
    // to disk, and pushing it over IPC to the driver.
    //
    // Returns true if the user changed any control this frame (so the caller
    // can act on it without diff-checking the struct).
    bool Render(protocol::SmoothingConfig &cfg, IPCClient &ipc, bool ipcConnected);
}
