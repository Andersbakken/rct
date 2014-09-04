#ifndef CPUUSAGE_H
#define CPUUSAGE_H

#include <cstdint>

// CPU usage over the last couple of seconds, range from 0 (idle) to 1 (100%)

class CpuUsage
{
public:
    static float usage();

private:
    CpuUsage() = delete;
    CpuUsage(const CpuUsage&) = delete;
    CpuUsage& operator=(const CpuUsage&) = delete;
};

#endif
