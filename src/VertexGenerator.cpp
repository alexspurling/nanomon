#include "VertexGenerator.h"
#include <algorithm>

VertexGenerator::VertexGenerator(
        std::function<double(int prev_idx, int curr_idx)> sample_fn,
        const int sample_window_size,
        const int max_vertices)
    : m_sample_fn(std::move(sample_fn))
    , m_min_sample_window_size(sample_window_size)
    , m_max_vertices(max_vertices) {}

std::vector<float> VertexGenerator::generate_vertices(
        const ViewWindow& window, const int num_samples) {
    const int step = window.calculate_step();
    const int sample_window_size = std::max(m_min_sample_window_size, step);
    const int data_width = window.view_width();

    // Store stats
    m_last_stats.total_samples = num_samples;
    m_last_stats.excess_samples = num_samples - window.view_end();
    m_last_stats.step          = step;
    m_last_stats.count         = 0;
    m_last_stats.data_width    = data_width;
    m_last_stats.start         = window.view_start();
    m_last_stats.end           = window.view_end();
    m_last_stats.scroll_offset = window.get_scroll_offset();

    // We need at least sample_window_size + step samples in order to create two points to form a line-segment
    if (num_samples <= sample_window_size + step) {
        m_last_stats = {};
        return {};
    }

    // Snap the sample indices to a multiple of the step
    const int first_idx = std::max(window.view_start() / step * step, sample_window_size);
    const int last_idx = std::min(window.view_end() / step * step, num_samples - 1);

    std::vector<float> vertices;

    // Expand the width of the screen by one division so we can ensure that we always draw lines that reach the boundary
    // of the canvas and prevent gaps at the edge of our graph
    const int visible_count = data_width / step;
    const double graph_screen_width = 2.0 * visible_count / (visible_count - 1);

    for (int sample_idx = first_idx; sample_idx <= last_idx; sample_idx += step) {
        const int prev_idx = std::max(0, sample_idx - sample_window_size);
        const double function_y = m_sample_fn(prev_idx, sample_idx);

        // Map sample index to NDC x in [-1, 1]
        const double t = (static_cast<double>(sample_idx) - window.view_start()) / data_width;
        const float x = static_cast<float>(t * graph_screen_width - 1.0);

        // Map value [0, 1] to NDC y [-1, 1]
        const float y = static_cast<float>(function_y) * 2.0f - 1.0f;

        vertices.push_back(x);
        vertices.push_back(y);
    }

    m_last_stats.count = static_cast<int>(vertices.size() / 2);

    return vertices;
}