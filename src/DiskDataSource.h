#pragma once

#include "DataSource.h"

class DiskDataSource : public DataSource {
public:
    DiskDataSource();

    [[nodiscard]] int num_series() const override;
    [[nodiscard]] const SeriesConfig& series_config(int index) const override;
    void sample() override;
    [[nodiscard]] const DataSeries& series(int index) const override;
    [[nodiscard]] int num_samples() const override;

private:
    DataSeries m_series;
    SeriesConfig m_config;
};