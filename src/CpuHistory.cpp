#include "CpuHistory.h"

#include <iostream>

CpuHistory::CpuHistory()
{
    const auto stat = m_sampler.sample();
    const size_t num_cores = stat.size();
    m_cpu_history.resize(num_cores);
}

void CpuHistory::sample(const Timestamp &timestamp) {
    const std::vector<CpuStat> cpu_stats = m_sampler.sample();

    // Record a CoreSample for each core
    for (int i = 0; i < static_cast<int>(cpu_stats.size()); i++) {
        const CpuStat& stat = cpu_stats[i];
        m_cpu_history[i].push_back(CoreSample(stat.total(), stat.idle));
    }
}

int CpuHistory::num_samples() const {
    return m_cpu_history[0].size();
}

const CoreSample& CpuHistory::sample_at(const int core_id, const int index) const {
    int sample_count = m_cpu_history[core_id].size();
    if (index < 0 || index >= sample_count) {
        std::cerr << "CpuHistory::sample_at: index out of range. index: " << index << ", range: 0-" << sample_count << std::endl;
    }
    return m_cpu_history[core_id][index];
}