#define _CRT_SECURE_NO_DEPRECATE
#include "Logging.h"

#include <chrono>
#include <cstdio>
#include <ctime>

FILE *LogFile = nullptr;

// Per-launch log file. Each driver load (vrserver picks the DLL up) opens a
// fresh file named `fingersmoothing_driver.YYYY-MM-DD_HH-MM-SS.log` next to
// the DLL. Append-mode-to-one-file would force the user to clear it manually
// every iteration; per-launch keeps each session's evidence isolated and the
// bug-report flow ("attach the most recent log file") trivial.
//
// Old files accumulate — disk usage is small (a typical M1 session log is
// ~1KB, an M3 session a few MB), and a manual prune is the right tool if it
// ever matters. SpaceCalibrator's overlay uses the same pattern under
// %LocalAppDataLow%\SpaceCalibrator\Logs and accumulates without bound.
void OpenLogFile()
{
    auto now = std::chrono::system_clock::now();
    auto nowTime = std::chrono::system_clock::to_time_t(now);
    tm value{};
    localtime_s(&value, &nowTime);

    char filename[64];
    std::snprintf(filename, sizeof filename,
        "fingersmoothing_driver.%04d-%02d-%02d_%02d-%02d-%02d.log",
        value.tm_year + 1900, value.tm_mon + 1, value.tm_mday,
        value.tm_hour, value.tm_min, value.tm_sec);

    LogFile = std::fopen(filename, "w");
    if (LogFile == nullptr)
    {
        // Fall back to stderr so a permission/path issue still produces
        // observable output (vrserver tends to swallow stderr, but at least
        // the LOG calls won't crash on a null FILE*).
        LogFile = stderr;
    }
}

tm TimeForLog()
{
    auto now = std::chrono::system_clock::now();
    auto nowTime = std::chrono::system_clock::to_time_t(now);
    tm value{};
    localtime_s(&value, &nowTime);
    return value;
}

void LogFlush()
{
    if (LogFile) std::fflush(LogFile);
}
