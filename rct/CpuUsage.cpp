#include "CpuUsage.h"

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <mutex>
#include <thread>
#include <cstdint>
#ifdef OS_Darwin
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/processor_info.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#endif
#if defined(OS_FreeBSD) || defined(OS_DragonFly)
#include <sys/sysctl.h>
#endif

#include "Rct.h"

#define SLEEP_TIME 1000000 // one second

struct CpuData
{
    std::mutex mutex;
    std::thread thread;

    uint32_t lastUsage;
    uint64_t lastTime;

    float usage;

#if defined(OS_Linux) || defined (OS_Darwin) || defined(OS_FreeBSD) || \
        defined(OS_DragonFly)
    float hz;
    uint32_t cores;
#endif
};

static CpuData sData;
static std::once_flag sFlag;

static int64_t currentUsage()
{
#if defined(OS_Linux)
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
#elif defined(OS_Darwin)
    processor_info_array_t cpuInfo;
    mach_msg_type_number_t numCpuInfo;
    natural_t numCPUs = 0;
    kern_return_t err = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &numCPUs, &cpuInfo, &numCpuInfo);
    if (err == KERN_SUCCESS) {
        int64_t usage = 0;
        for (unsigned int i = 0; i < numCPUs; ++i) {
            usage += cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_USER] + cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_SYSTEM] + cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_NICE];
        }
        const size_t cpuInfoSize = sizeof(integer_t) * numCpuInfo;
        vm_deallocate(mach_task_self(), (vm_address_t)cpuInfo, cpuInfoSize);
        return usage;
    }
    return -1;
#elif defined(OS_FreeBSD) || defined(OS_DragonFly)
    const size_t ncpu_mib_len = 2;
    const int ncpu_mib[ncpu_mib_len] = { CTL_HW, HW_NCPU };
    int ncpu;
    size_t ncpu_len = sizeof(ncpu);

    if (-1 == sysctl(ncpu_mib, ncpu_mib_len, &ncpu, &ncpu_len, NULL, 0)) {
        return -1;
    }

    unsigned long long total_usage = 0;

    for (int cpu_id = 0; cpu_id < ncpu; ++cpu_id) {
        size_t cpu_usage_mib_len = 4;
        int cpu_usage_mib[cpu_usage_mib_len];
        int cpu_usage = 0;
#if defined(OS_FreeBSD)
        const char *cpu_usage_sysctl_name_fmt = "dev.cpu.%d.cx_usage";
#else
        const char *cpu_usage_sysctl_name_fmt = "hw.acpi.cpu%d.cx_usage";
#endif
        size_t sysctl_entry_name_len = snprintf(NULL, 0, cpu_usage_sysctl_name_fmt, cpu_id) + 1;
        char *mib_name = (char*)malloc(sysctl_entry_name_len);
        snprintf(mib_name, sysctl_entry_name_len, cpu_usage_sysctl_name_fmt, cpu_id);

        if (-1 == sysctlnametomib(mib_name, cpu_usage_mib, &cpu_usage_mib_len)) {
            free(mib_name);
            return -1;
        }

        if (-1 == sysctl(cpu_usage_mib, 4, &cpu_usage, &cpu_usage_mib_len, NULL, 0)) {
            free(mib_name);
            return -1;
        }

        total_usage += cpu_usage;
        free(mib_name);
    }

    return total_usage;
#else
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
#if defined(OS_Linux) || defined(OS_Darwin) || defined(OS_FreeBSD) || \
        defined(OS_DragonFly)
                    const uint32_t deltaUsage = usage - sData.lastUsage;
                    const uint64_t deltaTime = time - sData.lastTime;
                    const float timeRatio = deltaTime / (SLEEP_TIME / 1000);
                    sData.usage = (deltaUsage / sData.hz / sData.cores) / timeRatio;
#endif
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
#if defined(OS_Linux) || defined(OS_Darwin) || defined(OS_FreeBSD) || \
        defined(OS_DragonFly)
            sData.hz = sysconf(_SC_CLK_TCK);
            sData.cores = sysconf(_SC_NPROCESSORS_ONLN);
#endif
            sData.thread = std::thread(collectData);
        });

    std::lock_guard<std::mutex> locker(sData.mutex);
    return 1. - sData.usage;
}
