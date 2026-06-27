#pragma once

#include <chrono>
#include "CpuTimesSampler.h"

using Timestamp = std::chrono::system_clock::time_point;

struct CoreSample {

    unsigned long total_time;
    unsigned long idle_time;

    CoreSample(long total, long idle)
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

    CpuSample sample(const Timestamp &timestamp);

    int num_samples() const;

    CpuSample prev_sample() const;

private:
    CpuTimesSampler m_sampler;
    std::vector<CpuSample> m_samples;  // new member
};
