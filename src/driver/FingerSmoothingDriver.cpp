#include "ServerTrackedDeviceProvider.h"
#include "VRWatchdogProvider.h"
#include "Logging.h"

#include <openvr_driver.h>

#include <cstring>

#if defined(_WIN32)
#define HMD_DLL_EXPORT extern "C" __declspec(dllexport)
#else
#define HMD_DLL_EXPORT extern "C" __attribute__((visibility("default")))
#endif

static ServerTrackedDeviceProvider g_serverDriver;
static VRWatchdogProvider          g_watchdog;

HMD_DLL_EXPORT void *HmdDriverFactory(const char *pInterfaceName, int *pReturnCode)
{
    if (std::strcmp(pInterfaceName, vr::IServerTrackedDeviceProvider_Version) == 0)
        return &g_serverDriver;
    if (std::strcmp(pInterfaceName, vr::IVRWatchdogProvider_Version) == 0)
        return &g_watchdog;

    LOG("HmdDriverFactory: unrecognized interface '%s'", pInterfaceName);
    if (pReturnCode)
        *pReturnCode = vr::VRInitError_Init_InterfaceNotFound;
    return nullptr;
}
