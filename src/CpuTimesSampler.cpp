#include "CpuTimesSampler.h"

#include <vector>
#include <string>
#include <fstream>
#include <sstream>


std::vector<CpuStat> CpuTimesSampler::sample() const {
    std::vector<CpuStat> result;

    std::ifstream file("/proc/stat");
    if (!file.is_open())
        return result;

    std::string line;

    while (std::getline(file, line)) {
        if (line.rfind("cpu", 0) != 0)
            continue;

        // skip aggregate "cpu " line if desired
        if (line.rfind("cpu ", 0) == 0)
            continue;

        std::istringstream iss(line);

        std::string cpu_label;
        CpuStat stat;

        iss >> cpu_label;
        iss >> stat.user
            >> stat.nice
            >> stat.system
            >> stat.idle
            >> stat.iowait
            >> stat.irq
            >> stat.softirq
            >> stat.steal;

        result.push_back(stat);
    }

    return result;
}