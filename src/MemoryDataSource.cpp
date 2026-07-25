#include "MemoryDataSource.h"

#include <fstream>
#include <sstream>
#include <string>
#include <nanogui/vector.h>

using nanogui::Vector3f;

MemoryDataSource::MemoryDataSource() {
    m_config.color  = {1.0f, 0.6f, 0.0f}; // orange
    m_config.label  = "Memory used%";
    m_config.y_min  = 0.0;
    m_config.y_max  = 1.0;
}

int MemoryDataSource::num_series() const {
    return 1;
}

const SeriesConfig& MemoryDataSource::series_config(int index) const {
    (void)index;
    return m_config;
}

void MemoryDataSource::sample() {
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) {
        m_series.add_sample(0.0);
        return;
    }

    unsigned long total = 0, available = 0;
    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("MemTotal:", 0) == 0) {
            std::istringstream iss(line);
            std::string key;
            iss >> key >> total;
        } else if (line.rfind("MemAvailable:", 0) == 0) {
            std::istringstream iss(line);
            std::string key;
            iss >> key >> available;
        }
        if (total > 0 && available > 0)
            break;
    }

    const double used_ratio = (total > 0)
        ? static_cast<double>(total - available) / static_cast<double>(total)
        : 0.0;
    m_series.add_sample(used_ratio);
}

const DataSeries& MemoryDataSource::series(int index) const {
    (void)index;
    return m_series;
}

int MemoryDataSource::num_samples() const {
    return m_series.num_samples();
}