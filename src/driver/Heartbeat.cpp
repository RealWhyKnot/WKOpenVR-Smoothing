#include "Heartbeat.h"
#include "Logging.h"

#include <chrono>

void Heartbeat::Start()
{
    stopFlag.store(false);
    worker = std::thread([this]() {
        int seconds = 0;
        while (!stopFlag.load()) {
            // Sleep in small slices so Stop() can return quickly without
            // waiting up to 5s for the next wake.
            for (int i = 0; i < 50 && !stopFlag.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (stopFlag.load()) break;
            seconds += 5;

            LOG("FS-HEARTBEAT t=%ds  skeletal=%llu  boolean=%llu  scalar=%llu  skelCreates=%llu  poseUpdates=%llu  iobufWrites=%llu  iobufOpens=%llu  genericIfaceQueries=%llu",
                seconds,
                (unsigned long long)hook_stats::g_skeletalHits.load(),
                (unsigned long long)hook_stats::g_booleanHits.load(),
                (unsigned long long)hook_stats::g_scalarHits.load(),
                (unsigned long long)hook_stats::g_skeletonCreates.load(),
                (unsigned long long)hook_stats::g_poseUpdates.load(),
                (unsigned long long)hook_stats::g_iobufferWrites.load(),
                (unsigned long long)hook_stats::g_iobufferOpens.load(),
                (unsigned long long)hook_stats::g_genericInterfaceQueries.load());
        }
    });
}

void Heartbeat::Stop()
{
    stopFlag.store(true);
    if (worker.joinable()) worker.join();
}
