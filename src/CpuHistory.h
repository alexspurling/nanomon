#pragma once

#include <chrono>
#include "CpuTimesSampler.h"

using Timestamp = std::chrono::system_clock::time_point;

struct CoreSample {

    unsigned long total_time;
    unsigned long idle_time;

    CoreSample(const long total, const long idle)
        : total_time(total), idle_time(idle) {
    }
};

struct CpuSample {

    Timestamp timestamp;
    std::vector<CoreSample> samples;

    CpuSample(const Timestamp& timestamp, std::vector<CoreSample> samples)
        : timestamp(timestamp), samples(std::move(samples)) {}
};

class CpuHistory
{
public:

    CpuHistory();

    void sample(const Timestamp &timestamp);

    int num_samples() const;

    const CoreSample& sample_at(int core_id, int index) const;

private:
    CpuTimesSampler m_sampler;
    std::vector<std::vector<CoreSample>> m_cpu_history;
};
