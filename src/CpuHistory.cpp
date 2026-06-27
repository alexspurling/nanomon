#include "CpuHistory.h"

CpuHistory::CpuHistory()
    : m_sampler()
{
}

CpuSample CpuHistory::sample(const Timestamp &timestamp)
{
    const std::vector<CpuStat> cpu_stats = m_sampler.sample();

    std::vector<CoreSample> samples;
    samples.reserve(cpu_stats.size());

    for (const CpuStat& stat : cpu_stats)
    {
        const unsigned long total_time = stat.user + stat.nice + stat.system + stat.idle + stat.iowait + stat.irq + stat.softirq + stat.steal;
        samples.push_back(CoreSample(total_time, stat.idle));
    }

    CpuSample latest_sample = CpuSample(timestamp, samples);
    m_samples.push_back(latest_sample);

    return latest_sample;
}

int CpuHistory::num_samples() const {
    return m_samples.size();
}

CpuSample CpuHistory::prev_sample() const {
    // TODO guard against empty or 1 element vector
    return m_samples[m_samples.size() - 2];
}