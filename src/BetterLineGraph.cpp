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
    , m_max_vertices(max_vertices){}

void BetterLineGraph::pan(const int num_samples, const double delta_idx) {
    std::cout << "pan: " << delta_idx << std::endl;
    set_view_window(num_samples, m_view_start + delta_idx, m_view_end + delta_idx);
}

void BetterLineGraph::zoom(const int num_samples, const double factor, const double mouse_ratio) {
    const double width = view_width();
    const double new_width = width * factor;
    if (width <= 2 && factor < 1.0) {
        return; // Can't zoom in beyond this
    }
    const double mouse_idx = m_view_start + mouse_ratio * width;

    const double new_view_start = mouse_idx - new_width * mouse_ratio;
    const double new_view_end = mouse_idx + new_width * (1.0 - mouse_ratio);

    set_view_window(num_samples, new_view_start, new_view_end);
    update_points(m_last_stats.total_samples);
}

void BetterLineGraph::set_view_window(const int num_samples, const double view_start, const double view_end) {
    // Set the start and end values so we can calculate the step at the new window size
    m_view_start = view_start;
    m_view_end   = view_end;
    const int step = calculate_step();
    const double max_end = std::min(view_end, static_cast<double>(num_samples - step));
    const double actual_start = max_end - (view_end - view_start);
    m_view_start = actual_start;
    m_view_end   = max_end;

    std::cout << "view_start: " << view_start << ", view_end: " << view_end <<
        ", actual_start: " << actual_start << ", max_end: " << max_end <<
        ", m_view_start: " << m_view_start << ", m_view_end: " << m_view_end << std::endl;
}

int BetterLineGraph::calculate_step() const {
    return std::max(1, static_cast<int>(view_width() / m_max_vertices));
}

void BetterLineGraph::add_sample(const int num_samples) {
    // Update the vertex data for this graph based on the new number of samples
    const int step = calculate_step();

    // Increment every time we get step samples plus one extra. We need the extra sample to produce a line-segment.
    // For example, if step == 2, then 5 samples are needed to generate 2 line-segments and 7 samples to generate 3 line-segments.
    if ((num_samples - 1) % step == 0) {
        m_view_end += step;
        m_view_start += step;
    }

    update_points(num_samples);
}

void BetterLineGraph::update_points(const int num_samples) {

    // Update the vertex data for this graph based on the new number of samples
    const int step = calculate_step();

    const int sample_window_size = std::max(m_min_sample_window_size, step);

    // We need at least sample_window_size + step samples in order to create two points to form a line-segment
    if (num_samples <= sample_window_size + step) {
        m_last_stats = {};
        return;
    }

    // Snap the sample indices to a multiple of the step
    const int first_idx = std::max(m_view_start / step * step, sample_window_size);
    const int last_idx = std::min(m_view_end / step * step, num_samples - 1);

    // const int visible_count = last_idx - first_idx + 1;
    // const int count = (visible_count + step - 1) / step; // ceil division

    const double data_width = view_width();

    // Store stats
    m_last_stats.total_samples = num_samples;
    m_last_stats.visible_count = 0;
    m_last_stats.step          = step;
    m_last_stats.count         = 0;
    m_last_stats.data_width    = data_width;
    m_last_stats.start         = m_view_start;
    m_last_stats.end           = m_view_end;

    // vertices.reserve(count * 2);

    m_vertices.clear();

    for (int sample_idx = first_idx; sample_idx <= last_idx; sample_idx += step) {
        const int prev_idx = std::max(0, sample_idx - sample_window_size);
        const double function_y = m_sample_fn(prev_idx, sample_idx);

        // Map sample index → NDC x in [-1, 1]
        const double t = (static_cast<double>(sample_idx) - m_view_start) / (data_width - 1);
        const float x = static_cast<float>(t * 2.0 - 1.0);

        // Map value [0, 1] → NDC y [-1, 1]
        const float y = static_cast<float>(function_y) * 2.0f - 1.0f;

        m_vertices.push_back(x);
        m_vertices.push_back(y);
    }

    m_last_stats.count = m_vertices.size() / 2;
}

void BetterLineGraph::update_scroll(const int num_samples, const double sample_progress) {
    // Update the vertex data for this graph based on the new number of samples
    const int step = calculate_step();

    // sample_progress is a number between 0 and 1 representing how far we are between two sample intervals
    // scroll_offset is a number between 0 and step representing how far we are between two step intervals
    double scroll_offset = (num_samples - 1) % step + sample_progress;
    const double data_width = view_width();
    m_scroll_offset = scroll_offset * 2.0 / data_width;
}
