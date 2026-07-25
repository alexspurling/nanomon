#pragma once

#include <nanogui/vector.h>
#include <string>
#include "DataSeries.h"

struct SeriesConfig {
    nanogui::Vector3f color;
    std::string label;
    double y_min = 0.0;
    double y_max = 1.0;
    bool right_axis = false;
};

class DataSource {
public:
    virtual ~DataSource() = default;

    /** Number of data series (lines) this source provides. */
    [[nodiscard]] virtual int num_series() const = 0;

    /** Per-series visual configuration. */
    [[nodiscard]] virtual const SeriesConfig& series_config(int index) const = 0;

    /** Pull new data; pushes one sample to each series. */
    virtual void sample() = 0;

    /** Access the underlying time-series data for a given series. */
    [[nodiscard]] virtual const DataSeries& series(int index) const = 0;

    /** Total number of samples stored (all series share the same count). */
    [[nodiscard]] virtual int num_samples() const = 0;

    /**
     * Compute a display value for a range of samples.
     * Default: return the value at curr_idx (simple lookup).
     * Override for sources that need windowed computation (e.g. CPU utilization).
     */
    [[nodiscard]] virtual double compute_value(int series_idx, int prev_idx, int curr_idx) const {
        (void)prev_idx;
        return series(series_idx).sample_at(curr_idx);
    }
};