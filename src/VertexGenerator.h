#pragma once

#include <vector>
#include <functional>

#include "ViewWindow.h"

class VertexGenerator {
public:
    VertexGenerator(std::function<double(int prev_idx, int curr_idx)> sample_fn,
                    int sample_window_size = 2,
                    int max_vertices = 50,
                    double y_min = 0.0,
                    double y_max = 1.0);

    /**
     * Generate vertex data for the given view window.
     * Returns a vector of interleaved (x, y) pairs in NDC space.
     */
    std::vector<float> generate_vertices(const ViewWindow& window, int num_samples);

    void set_y_range(double y_min, double y_max) {
        m_y_min = y_min;
        m_y_max = y_max;
    }

private:
    std::function<double(int prev_idx, int curr_idx)> m_sample_fn;
    int m_min_sample_window_size;
    int m_max_vertices;
    double m_y_min;
    double m_y_max;
};