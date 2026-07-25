#include "CpuDataSource.h"

#include <nanogui/vector.h>

using nanogui::Vector3f;

static const Vector3f CORE_COLOURS[8] = {
    {0.0f, 1.0f, 0.0f},  // green
    {1.0f, 0.0f, 0.0f},  // red
    {0.0f, 0.5f, 1.0f},  // light blue
    {1.0f, 1.0f, 0.0f},  // yellow
    {1.0f, 0.0f, 1.0f},  // magenta
    {0.0f, 1.0f, 1.0f},  // cyan
    {1.0f, 0.5f, 0.0f},  // orange
    {0.5f, 0.0f, 1.0f},  // purple
};

CpuDataSource::CpuDataSource() {
    // Discover core count from /proc/stat
    const auto stat = m_sampler.sample();
    const int num_cores = static_cast<int>(stat.size());

    m_series.resize(num_cores);
    m_configs.resize(num_cores);
    m_total_history.resize(num_cores);
    m_idle_history.resize(num_cores);

    for (int i = 0; i < num_cores; ++i) {
        m_configs[i].color = CORE_COLOURS[i % 8];
        m_configs[i].label = "cpu" + std::to_string(i);
        m_configs[i].y_min = 0.0;
        m_configs[i].y_max = 1.0;
    }
}

int CpuDataSource::num_series() const {
    return static_cast<int>(m_series.size());
}

const SeriesConfig& CpuDataSource::series_config(int index) const {
    return m_configs[index];
}

void CpuDataSource::sample() {
    const auto stats = m_sampler.sample();
    const int n = static_cast<int>(stats.size());

    for (int i = 0; i < n; ++i) {
        const double total = static_cast<double>(stats[i].total());
        const double idle  = static_cast<double>(stats[i].idle);
        m_total_history[i].push_back(total);
        m_idle_history[i].push_back(idle);
        // Store a dummy — the real value is computed in compute_value()
        m_series[i].add_sample(0.0);
    }
}

const DataSeries& CpuDataSource::series(const int index) const {
    return m_series[index];
}

int CpuDataSource::num_samples() const {
    return m_series.empty() ? 0 : m_series[0].num_samples();
}

double CpuDataSource::compute_value(const int series_idx, const int prev_idx, const int curr_idx) const {
    const double total_diff = m_total_history[series_idx][curr_idx] - m_total_history[series_idx][prev_idx];
    const double idle_diff  = m_idle_history[series_idx][curr_idx]  - m_idle_history[series_idx][prev_idx];
    return (total_diff > 0.0)
        ? (total_diff - idle_diff) / total_diff
        : 0.0;
}