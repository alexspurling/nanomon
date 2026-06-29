#pragma once

#include <vector>

struct CpuStat {
    unsigned long user = 0;
    unsigned long nice = 0;
    unsigned long system = 0;
    unsigned long idle = 0;
    unsigned long iowait = 0;
    unsigned long irq = 0;
    unsigned long softirq = 0;
    unsigned long steal = 0;

    unsigned long total() const { return user + nice + system + idle + iowait + irq + softirq + steal; }
};

class CpuTimesSampler {
public:
    // Returns one CpuStat per CPU core (cpu0, cpu1, ...)
    static std::vector<CpuStat> sample();
};