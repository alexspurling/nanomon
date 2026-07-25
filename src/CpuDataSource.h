#pragma once

#include "DataSource.h"
#include "CpuTimesSampler.h"
#include <vector>

class CpuDataSource : public DataSource {
public:
    CpuDataSource();

    [[nodiscard]] int num_series() const override;
    [[nodiscard]] const SeriesConfig& series_config(int index) const override;
    void sample() override;
    [[nodiscard]] const DataSeries& series(int index) const override;
    [[nodiscard]] int num_samples() const override;
    [[nodiscard]] double compute_value(int series_idx, int prev_idx, int curr_idx) const override;

private:
    std::vector<DataSeries> m_series;
    std::vector<SeriesConfig> m_configs;
    CpuTimesSampler m_sampler;
    // Raw total_time per core per sample (for computing utilization over windows)
    std::vector<std::vector<double>> m_total_history;
    std::vector<std::vector<double>> m_idle_history;
};