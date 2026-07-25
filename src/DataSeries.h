#pragma once

#include <vector>

class DataSeries {
public:
    void add_sample(double value) { m_samples.push_back(value); }
    [[nodiscard]] double sample_at(int index) const { return m_samples[index]; }
    [[nodiscard]] int num_samples() const { return static_cast<int>(m_samples.size()); }

private:
    std::vector<double> m_samples;
};