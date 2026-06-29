#pragma once

#include <chrono>
#include "CpuTimesSampler.h"

using Timestamp = std::chrono::system_clock::time_point;

struct CoreSample {

    double total_time;
    double idle_time;

    CoreSample(const double total, const double idle)
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
