#include "ServerTrackedDeviceProvider.h"
#include "InterfaceHookInjector.h"
#include "Logging.h"
#include "Version.h"

vr::EVRInitError ServerTrackedDeviceProvider::Init(vr::IVRDriverContext *pDriverContext)
{
    LOG("FingerSmoothing driver " FINGERSMOOTH_VERSION_STRING " Init()");
    VR_INIT_SERVER_DRIVER_CONTEXT(pDriverContext);

    // Default-initialize config to "passthrough mode": master OFF, all fingers
    // enabled-in-mask, paper-default One-Euro params, adaptive off. The UI sends
    // a SetConfig the moment it connects; this is the safe pre-connect state
    // (zero behaviour change for any consuming OpenVR app until the user opts in).
    {
        std::lock_guard<std::mutex> lock(configMutex);
        config.master_enabled  = false;
        config.mincutoff       = 1.0f;
        config.beta            = 0.007f;
        config.dcutoff         = 1.0f;
        config.finger_mask     = protocol::kAllFingersMask;
        config.adaptive_enabled = false;
    }

    InjectHooks(this, pDriverContext);
    ipc.Run();
    heartbeat.Start();

    return vr::VRInitError_None;
}

void ServerTrackedDeviceProvider::Cleanup()
{
    LOG("FingerSmoothing driver Cleanup()");
    heartbeat.Stop();
    ipc.Stop();
    DisableHooks();
    VR_CLEANUP_SERVER_DRIVER_CONTEXT();
}

void ServerTrackedDeviceProvider::SetConfig(const protocol::SmoothingConfig &cfg)
{
    std::lock_guard<std::mutex> lock(configMutex);
    config = cfg;
}

protocol::SmoothingConfig ServerTrackedDeviceProvider::GetConfig() const
{
    std::lock_guard<std::mutex> lock(configMutex);
    return config;
}
