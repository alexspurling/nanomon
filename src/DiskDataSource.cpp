#include "DiskDataSource.h"

#include <sys/statvfs.h>
#include <nanogui/vector.h>

using nanogui::Vector3f;

DiskDataSource::DiskDataSource() {
    m_config.color  = {0.3f, 0.7f, 1.0f}; // light blue
    m_config.label  = "Disk used%";
    m_config.y_min  = 0.0;
    m_config.y_max  = 1.0;
}

int DiskDataSource::num_series() const {
    return 1;
}

const SeriesConfig& DiskDataSource::series_config(int index) const {
    (void)index;
    return m_config;
}

void DiskDataSource::sample() {
    struct statvfs stat;
    if (statvfs("/", &stat) != 0) {
        m_series.add_sample(0.0);
        return;
    }

    const double total = static_cast<double>(stat.f_blocks) * stat.f_frsize;
    const double available = static_cast<double>(stat.f_bavail) * stat.f_frsize;
    const double used_ratio = (total > 0.0)
        ? (total - available) / total
        : 0.0;
    m_series.add_sample(used_ratio);
}

const DataSeries& DiskDataSource::series(int index) const {
    (void)index;
    return m_series;
}

int DiskDataSource::num_samples() const {
    return m_series.num_samples();
}