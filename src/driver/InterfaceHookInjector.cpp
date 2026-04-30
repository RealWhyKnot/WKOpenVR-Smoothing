#include "InterfaceHookInjector.h"
#include "Logging.h"
#include "Hooking.h"
#include "ServerTrackedDeviceProvider.h"

#include <unordered_map>

static ServerTrackedDeviceProvider *Driver = nullptr;

// Hook on the driver context's GetGenericInterface. Used as a safety net: if
// the direct vtable hook in InjectHooks() can't grab IVRDriverInput at init
// time (e.g. it isn't initialised in this driver's SDK cache yet), this
// detour catches the singleton when the next driver in vrserver.exe queries
// it. With our `01fingersmoothing` driver name we load before Index, so this
// path fires when Index's driver queries IVRDriverInput_003.
static Hook<void *(*)(vr::IVRDriverContext *, const char *, vr::EVRInitError *)>
    GetGenericInterfaceHook("IVRDriverContext::GetGenericInterface");

// Skeletal-update interception. IVRDriverInput is a process-wide singleton
// inside vrserver.exe; vtable-hooking the UpdateSkeletonComponent slot on it
// catches every driver's call (Index's, ours, anyone's). Order in the vtable
// (from openvr_driver.h):
//   0 CreateBooleanComponent
//   1 UpdateBooleanComponent
//   2 CreateScalarComponent
//   3 UpdateScalarComponent
//   4 CreateHapticComponent
//   5 CreateSkeletonComponent
//   6 UpdateSkeletonComponent  <- target
static constexpr int kUpdateSkeletonComponentVTableIndex = 6;

static Hook<vr::EVRInputError(*)(vr::IVRDriverInput *, vr::VRInputComponentHandle_t, vr::EVRSkeletalMotionRange, const vr::VRBoneTransform_t *, uint32_t)>
    UpdateSkeletonComponentHook("IVRDriverInput003::UpdateSkeletonComponent");

// Throttled logging keyed by skeleton-component handle. The driver gets one
// handle per controller side per skeleton (left, right). At ~90 Hz we'd
// flood the log without throttling; log on first call and every 100th
// thereafter (~1/sec) so M1 verification has a steady heartbeat without
// drowning the log.
static std::unordered_map<vr::VRInputComponentHandle_t, uint64_t> g_skeletalCallCounts;

static vr::EVRInputError DetourUpdateSkeletonComponent(
    vr::IVRDriverInput *_this,
    vr::VRInputComponentHandle_t ulComponent,
    vr::EVRSkeletalMotionRange eMotionRange,
    const vr::VRBoneTransform_t *pTransforms,
    uint32_t unTransformCount)
{
    uint64_t &count = g_skeletalCallCounts[ulComponent];

    // M1 logging: confirm the hook fires for Index. M3 will replace this with
    // the derive/smooth/reconstruct pipeline gated on Driver->GetConfig().
    if (count == 0 || (count % 100) == 0) {
        if (pTransforms && unTransformCount > 1) {
            LOG("FS-M1 UpdateSkeletonComponent: handle=%llu motionRange=%d boneCount=%u call#%llu bone1.pos=(%.4f,%.4f,%.4f) bone1.rot=(%.4f,%.4f,%.4f,%.4f)",
                (unsigned long long)ulComponent,
                (int)eMotionRange,
                unTransformCount,
                (unsigned long long)count,
                pTransforms[1].position.v[0],
                pTransforms[1].position.v[1],
                pTransforms[1].position.v[2],
                pTransforms[1].orientation.w,
                pTransforms[1].orientation.x,
                pTransforms[1].orientation.y,
                pTransforms[1].orientation.z);
        } else {
            LOG("FS-M1 UpdateSkeletonComponent: handle=%llu motionRange=%d boneCount=%u call#%llu (transforms null or boneCount<2)",
                (unsigned long long)ulComponent,
                (int)eMotionRange,
                unTransformCount,
                (unsigned long long)count);
        }
    }
    count++;

    return UpdateSkeletonComponentHook.originalFunc(_this, ulComponent, eMotionRange, pTransforms, unTransformCount);
}

// Try to install the vtable hook on a known-good IVRDriverInput pointer. Safe
// to call multiple times — IHook::Exists short-circuits if already installed.
static void TryInstallSkeletalHook(void *iface)
{
    if (!iface) return;
    if (IHook::Exists(UpdateSkeletonComponentHook.name)) return;

    UpdateSkeletonComponentHook.CreateHookInObjectVTable(iface, kUpdateSkeletonComponentVTableIndex, &DetourUpdateSkeletonComponent);
    IHook::Register(&UpdateSkeletonComponentHook);
}

static void *DetourGetGenericInterface(vr::IVRDriverContext *_this, const char *pchInterfaceVersion, vr::EVRInitError *peError)
{
    auto originalInterface = GetGenericInterfaceHook.originalFunc(_this, pchInterfaceVersion, peError);

    if (pchInterfaceVersion && std::string(pchInterfaceVersion) == "IVRDriverInput_003") {
        TryInstallSkeletalHook(originalInterface);
    }

    return originalInterface;
}

void InjectHooks(ServerTrackedDeviceProvider *driver, vr::IVRDriverContext *pDriverContext)
{
    Driver = driver;

    auto err = MH_Initialize();
    if (err != MH_OK) {
        LOG("MH_Initialize error: %s", MH_StatusToString(err));
        return;
    }

    // Install GetGenericInterface safety-net detour first. Catches any future
    // driver's interface query for IVRDriverInput_003.
    GetGenericInterfaceHook.CreateHookInObjectVTable(pDriverContext, 0, &DetourGetGenericInterface);
    IHook::Register(&GetGenericInterfaceHook);

    // Try to install the skeletal hook eagerly via the SDK's accessor. If our
    // driver SDK already cached an IVRDriverInput pointer, this hooks it
    // immediately. If not, the GetGenericInterface detour above will catch it
    // later.
    if (auto *input = vr::VRDriverInput()) {
        TryInstallSkeletalHook(input);
    }
}

void DisableHooks()
{
    IHook::DestroyAll();
    MH_Uninitialize();
}
