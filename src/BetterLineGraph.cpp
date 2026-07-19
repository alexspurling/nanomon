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

void BetterLineGraph::pan(const double delta_idx) {
    std::cout << "pan: " << delta_idx << std::endl;
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
    update_points();
}

void BetterLineGraph::set_view_window(const double view_start, const double view_end) {

    const int step = calculate_step(static_cast<int>(view_start), static_cast<int>(view_end));

    // Find the largest multiple of step that is less than or equal to (m_num_samples - step)
    // We subtract one step from the max value to always ensure that we are rendering one line
    // segment beyond the right-hand side of the graph
    const int max_end = (m_num_samples / step - 1) * step;
    // Requested end rounded to the nearest step
    m_view_end = std::min(max_end, static_cast<int>(view_end / step) * step);
    m_view_start = m_view_end - static_cast<int>((view_end - view_start) / step) * step;

    const double original_half_width = (view_end - view_start) / 2.0;
    const double original_center = view_start + original_half_width;
    const double offset_by = view_start - m_view_start;

    // std::cout << "pan offset: " << m_pan_offset << std::endl;

    const double new_offset_by = ((view_start + view_end) - (m_view_start + m_view_end)) / 2.0;

    const double actual_center = m_view_start + (m_view_end - m_view_start) / 2.0 + new_offset_by;

    m_pan_offset += new_offset_by;

    std::cout << "original center: " << original_center << ", actual center: " << actual_center <<
        ", new_offset_by: " << new_offset_by << ", total_offset: " << m_pan_offset <<
        std::endl;

    // std::cout << "view_start: " << view_start << ", view_end: " << view_end <<
    //     ", actual_start: " << actual_start << ", max_end: " << max_end <<
    //     ", m_view_start: " << m_view_start << ", m_view_end: " << m_view_end << std::endl;
}

void BetterLineGraph::set_view_start(const int start) {
    m_view_start = start;
    m_last_stats.start = start;
}

void BetterLineGraph::set_view_end(const int end) {
    m_view_end = end;
    m_last_stats.end = end;
}

int BetterLineGraph::calculate_step(const int start, const int end) const {
    return std::max(1, (end - start) / m_max_vertices);
}

void BetterLineGraph::add_sample() {
    const int step = calculate_step(m_view_start, m_view_end);

    // Increment every time we get step samples plus one extra. We need the extra sample to produce a line-segment.
    // For example, if step == 2, then 5 samples are needed to generate 2 line-segments and 7 samples to generate 3 line-segments.
    if ((m_num_samples - 1) % step == 0) {
        m_view_end += step;
        m_view_start += step;
    }

    // Update the vertex data for this graph based on the new number of samples
    update_points();
}

void BetterLineGraph::update_points() {

    // Update the vertex data for this graph based on the new number of samples
    const int step = calculate_step(m_view_start, m_view_end);

    const int sample_window_size = std::max(m_min_sample_window_size, step);

    // const int visible_count = last_idx - first_idx + 1;
    // const int count = (visible_count + step - 1) / step; // ceil division

    const int data_width = view_width();

    // Store stats
    m_last_stats.total_samples = m_num_samples;
    m_last_stats.excess_samples = m_num_samples - m_view_end;
    m_last_stats.step          = step;
    m_last_stats.count         = 0;
    m_last_stats.data_width    = data_width;
    m_last_stats.start         = m_view_start;
    m_last_stats.end           = m_view_end;
    m_last_stats.graph_width   = m_scroll_offset;

    // We need at least sample_window_size + step samples in order to create two points to form a line-segment
    if (m_num_samples <= sample_window_size + step) {
        m_last_stats = {};
        return;
    }

    // Snap the sample indices to a multiple of the step
    const int first_idx = std::max(m_view_start / step * step, sample_window_size);
    const int last_idx = std::min(m_view_end / step * step, m_num_samples - 1);

    // vertices.reserve(count * 2);

    m_vertices.clear();

    // Expand the width of the screen by one division so we can ensure that we always draw lines that reach the boundary
    // of the canvas and prevent gaps at the edge of our graph
    const int visible_count = data_width / step;
    const double graph_screen_width = 2.0 * visible_count / (visible_count - 1);

    for (int sample_idx = first_idx; sample_idx <= last_idx; sample_idx += step) {
        const int prev_idx = std::max(0, sample_idx - sample_window_size);
        const double function_y = m_sample_fn(prev_idx, sample_idx);

        // Map sample index → NDC x in [-1, 1]
        const double t = (static_cast<double>(sample_idx) - m_view_start) / data_width;
        const float x = static_cast<float>(t * graph_screen_width - 1.0);

        // Map value [0, 1] → NDC y [-1, 1]
        const float y = static_cast<float>(function_y) * 2.0f - 1.0f;

        m_vertices.push_back(x);
        m_vertices.push_back(y);
    }

    m_last_stats.count = m_vertices.size() / 2;

    // std::cout << "visible_count: " << visible_count << ", graph_screen_width: " << graph_screen_width <<
    //     ", x n-2: " << m_vertices[m_vertices.size() - 4] << ", x n-1: " << m_vertices[m_vertices.size() - 2] << std::endl;
}

void BetterLineGraph::update_scroll(const double sample_progress) {
    // Update the vertex data for this graph based on the new number of samples
    const int step = calculate_step(m_view_start, m_view_end);

    // sample_progress is a number between 0 and 1 representing how far we are between two sample intervals
    // scroll_offset is a number between 0 and step representing how far we are between two step intervals
    double scroll_offset = (m_num_samples - 1) % step + sample_progress + m_pan_offset;
    const int data_width = view_width();

    const int visible_count = data_width / step;
    const double graph_screen_width = 2.0 * visible_count / (visible_count - 1);
    m_scroll_offset = scroll_offset * graph_screen_width / data_width;
    m_last_stats.graph_width = m_scroll_offset;
}
