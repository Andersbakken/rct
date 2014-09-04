#include "CpuUsage.h"
#include "Rct.h"
#include <thread>
#include <mutex>
#include <unistd.h>
#include <assert.h>

#define SLEEP_TIME 1000000 // one second

struct CpuData
{
    std::mutex mutex;
    std::thread thread;

    uint32_t lastUsage;
    uint64_t lastTime;

    float usage;

#ifdef OS_Linux
    float hz;
    uint32_t cores;
#endif
};

static CpuData sData;
static std::once_flag sFlag;

static int64_t currentUsage()
{
#ifdef OS_Linux
    FILE* f = fopen("/proc/stat", "r");
    if (!f)
        return -1;
    char cpu[20];
    uint32_t user, nice, system, idle;
    if (fscanf(f, "%s\t%u\t%u\t%u\t%u\t", cpu, &user, &nice, &system, &idle) != 5) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return idle;
#else
#warning "CpuUsage not implemented for this platform"
    return -1;
#endif
}

static void collectData()
{
    for (;;) {
        const int64_t usage = currentUsage();
        if (usage == -1)
            break;
        const uint64_t time = Rct::monoMs();

        {
            std::lock_guard<std::mutex> locker(sData.mutex);
            assert(sData.lastTime < time);
            if (sData.lastTime > 0) {
                // did we wrap? if so, make load be 1 for now
                if (sData.lastUsage > usage) {
                    sData.usage = 0;
                } else {
                    const uint32_t deltaUsage = usage - sData.lastUsage;
                    const uint64_t deltaTime = time - sData.lastTime;
                    const float timeRatio = deltaTime / (SLEEP_TIME / 1000);
                    sData.usage = (deltaUsage / sData.hz / sData.cores) / timeRatio;
                }
            }
            sData.lastUsage = usage;
            sData.lastTime = time;
        }

        usleep(SLEEP_TIME);
    }
}

float CpuUsage::usage()
{
    std::call_once(sFlag, []() {
            std::lock_guard<std::mutex> locker(sData.mutex);
            sData.usage = 0;
            sData.lastUsage = 0;
            sData.lastTime = 0;
#ifdef OS_Linux
            sData.hz = sysconf(_SC_CLK_TCK);
            sData.cores = sysconf(_SC_NPROCESSORS_ONLN);
#endif
            sData.thread = std::thread(collectData);
        });

    std::lock_guard<std::mutex> locker(sData.mutex);
    return 1. - sData.usage;
}
