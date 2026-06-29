#include "CpuHistory.h"

CpuHistory::CpuHistory()
{
    auto stat = m_sampler.sample();
    size_t num_cores = stat.size();
    m_cpu_history.resize(num_cores);
}

void CpuHistory::sample(const Timestamp &timestamp) {
    const std::vector<CpuStat> cpu_stats = m_sampler.sample();

    // Record a CoreSample for each core
    for (int i = 0; i < static_cast<int>(cpu_stats.size()); i++) {
        const CpuStat& stat = cpu_stats[i];
        const unsigned long total_time = stat.user + stat.nice + stat.system + stat.idle + stat.iowait + stat.irq + stat.softirq + stat.steal;
        m_cpu_history[i].push_back(CoreSample(total_time, stat.idle));
    }
}

int CpuHistory::num_samples() const {
    return m_cpu_history[0].size();
}

const CoreSample& CpuHistory::sample_at(const int core_id, const int index) const {
    return m_cpu_history[core_id][index];
}