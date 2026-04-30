#pragma once

#include <openvr_driver.h>

#include "IPCServer.h"
#include "Protocol.h"

#include <mutex>

class ServerTrackedDeviceProvider : public vr::IServerTrackedDeviceProvider
{
public:
    // IServerTrackedDeviceProvider
    vr::EVRInitError Init(vr::IVRDriverContext *pDriverContext) override;
    void Cleanup() override;
    const char * const *GetInterfaceVersions() override
    {
        return vr::k_InterfaceVersions;
    }
    void RunFrame() override { }
    bool ShouldBlockStandbyMode() override { return false; }
    void EnterStandby() override { }
    void LeaveStandby() override { }

    // IPC handler — called from the IPC worker thread when the overlay sends
    // a SetConfig request. Atomic so the hook detour can read it without a
    // mutex on the per-frame skeletal-update hot path.
    void SetConfig(const protocol::SmoothingConfig &cfg);

    // Read the current config snapshot. Used by the skeletal-hook detour.
    protocol::SmoothingConfig GetConfig() const;

private:
    // Single-writer (IPC thread) / many-reader (skeletal hook). The skeletal
    // hook reads this once per UpdateSkeletonComponent call (~90Hz × 2 hands);
    // a mutex's contention cost is negligible at that rate, and the writer
    // (IPC thread, on user input) is rare. If the read frequency ever climbs
    // an order of magnitude, switch to a std::atomic<SmoothingConfig*> swap.
    mutable std::mutex configMutex;
    protocol::SmoothingConfig config;

    IPCServer ipc{this};
};
