#include "Logging.h"
#include "Version.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>

// Best-effort process-name lookup for the load/unload log lines. Different
// SteamVR components load the driver into their own processes (vrserver.exe,
// vrwatchdog.exe, occasionally vrcompositor.exe), so logging "loaded by pid X
// / proc.exe" disambiguates the load/unload cluster the last log showed.
static void LogProcInfo(const char *event)
{
	DWORD pid = GetCurrentProcessId();
	char procName[MAX_PATH] = "(unknown)";
	HMODULE me = GetModuleHandleA(nullptr);
	if (me) {
		char fullPath[MAX_PATH];
		DWORD got = GetModuleFileNameA(me, fullPath, MAX_PATH);
		if (got > 0 && got < MAX_PATH) {
			const char *base = strrchr(fullPath, '\\');
			snprintf(procName, sizeof procName, "%s", base ? base + 1 : fullPath);
		}
	}
	LOG("FingerSmoothing driver %s pid=%lu proc=%s", event, (unsigned long)pid, procName);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		OpenLogFile();
		LOG("FingerSmoothing driver " FINGERSMOOTH_VERSION_STRING " loaded");
		LogProcInfo("ATTACH");
		break;
	case DLL_PROCESS_DETACH:
		LogProcInfo("DETACH");
		LOG("FingerSmoothing driver unloaded");
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}

