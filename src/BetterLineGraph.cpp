#include "BetterLineGraph.h"
#include <algorithm>
#include <cmath>
#include <iostream>

BetterLineGraph::BetterLineGraph(
        const int core_id,
        std::function<double(int prev_idx, int curr_idx)> sample_fn,
        const int sample_window_size,
        const int max_vertices)
    : m_core_id(core_id)
    , m_sample_fn(std::move(sample_fn))
    , m_min_sample_window_size(sample_window_size)
    , m_max_vertices(max_vertices) {}

void BetterLineGraph::pan(const double delta_idx) {
    set_view_window(m_view_start + delta_idx, m_view_end + delta_idx);
}

void BetterLineGraph::zoom(const double factor, const double mouse_ratio) {
    const double width = view_width();
    const double new_width = width * factor;
    if (width <= 2 && factor < 1.0) {
        return; // Can't zoom in beyond this
    }
    const double mouse_idx = m_view_start + mouse_ratio * width;

    const double new_view_start = mouse_idx - new_width * mouse_ratio;
    const double new_view_end = mouse_idx + new_width * (1.0 - mouse_ratio);

    set_view_window(new_view_start, new_view_end);
    m_view_start = static_cast<int>(new_view_start);
    m_view_end = static_cast<int>(new_view_end);
}

void BetterLineGraph::set_view_window(const double start_idx, const double end_idx) {
    m_view_start = start_idx;
    m_view_end   = end_idx;
}

int BetterLineGraph::compute_step() const {
    // Step spans (window width / vertex budget) sample indices, so the
    // number of emitted vertices never exceeds max_vertices.
    return std::max(1, static_cast<int>(view_width() / m_max_vertices));
}

void BetterLineGraph::scroll_step(const int num_samples) {
    const int step = compute_step();

    // Increment every time we get step samples plus one extra. We need the extra sample to produce a line-segment.
    // For example, if step == 2, then 5 samples are needed to generate 2 line-segments and 7 samples to generate 3 line-segments.
    if ((num_samples - 1) % step == 0) {
        m_view_end += step;
        m_view_start += step;
    }
}

std::vector<float> BetterLineGraph::generate_vertices(const int total_samples) {

    const int step = compute_step();
    const int sample_window_size = std::max(m_min_sample_window_size, step);

    // We need at least sample_window_size + step samples in order to create two points to form a line-segment
    if (total_samples <= sample_window_size + step) {
        m_last_stats = {};
        return {};
    }

    // Snap the sample indices to a multiple of the step
    const int first_idx = std::max(m_view_start / step * step, sample_window_size);
    const int last_idx = std::min(m_view_end / step * step, total_samples - 1);

    // const int visible_count = last_idx - first_idx + 1;
    // const int count = (visible_count + step - 1) / step; // ceil division

    const double data_width = view_width();

    // Store stats
    m_last_stats.total_samples = total_samples;
    m_last_stats.visible_count = 0;
    m_last_stats.step          = step;
    m_last_stats.count         = 0;
    m_last_stats.data_width    = data_width;
    m_last_stats.start         = m_view_start;
    m_last_stats.end           = m_view_end;

    std::vector<float> vertices;
    // vertices.reserve(count * 2);

    for (int sample_idx = first_idx; sample_idx <= last_idx; sample_idx += step) {
        const int prev_idx = std::max(0, sample_idx - sample_window_size);
        const double function_y = m_sample_fn(prev_idx, sample_idx);

        // Map sample index → NDC x in [-1, 1]
        const double t = (static_cast<double>(sample_idx) - m_view_start) / data_width;
        const float x = static_cast<float>(t * 2.0 - 1.0);

        // Map value [0, 1] → NDC y [-1, 1]
        const float y = static_cast<float>(function_y) * 2.0f - 1.0f;

        vertices.push_back(x);
        vertices.push_back(y);
    }

    m_last_stats.count = vertices.size() / 2;

    return vertices;
}