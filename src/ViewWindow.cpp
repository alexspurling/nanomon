#include "ViewWindow.h"
#include <algorithm>
#include <cmath>

ViewWindow::ViewWindow(const int max_vertices)
    : m_max_vertices(max_vertices){}

void ViewWindow::pan(const double delta_idx) {
    // Pan relative to the true fractional position so sub-sample deltas accumulate
    const double view_start = m_view_start + m_pan_offset;
    const double view_end = m_view_end + m_pan_offset;
    set_view_window(view_start + delta_idx, view_end + delta_idx);
    update_offset();
}

void ViewWindow::zoom(const double factor, const double mouse_ratio) {
    const double width = view_width();
    const double new_width = width * factor;
    if (width <= 2 && factor < 1.0) {
        return; // Can't zoom in beyond this
    }
    const double mouse_idx = m_view_start + m_pan_offset + mouse_ratio * width;

    const double new_view_start = mouse_idx - new_width * mouse_ratio;
    const double new_view_end = mouse_idx + new_width * (1.0 - mouse_ratio);

    set_view_window(new_view_start, new_view_end);
    update_offset();
}

void ViewWindow::set_view_window(const double view_start, const double view_end) {
    // The view bounds are stored as integers; the fractional remainder of the
    // start position is kept in m_pan_offset (always in [0, 1)) and applied as
    // a sub-sample x offset by update_scroll.
    const double start_floor = std::floor(view_start);
    m_view_start = static_cast<int>(start_floor);
    m_pan_offset = view_start - start_floor;
    // Preserve the requested width rather than flooring both ends independently
    m_view_end = m_view_start + static_cast<int>(std::lround(view_end - view_start));

    m_graph_stats.step = calculate_step();
    m_graph_stats.data_width = view_width();
    m_graph_stats.start = this->view_start();
    m_graph_stats.end = this->view_end();
}

void ViewWindow::set_view_start(const int start) {
    m_view_start = start;
}

void ViewWindow::set_view_end(const int end) {
    m_view_end = end;
}

int ViewWindow::calculate_step() const {
    return std::max(1, view_width() / m_max_vertices);
}

void ViewWindow::add_sample() {
    const int step = calculate_step();

    // Increment every time we get step samples plus one extra. We need the extra sample to produce a line-segment.
    // For example, if step == 2, then 5 samples are needed to generate 2 line-segments and 7 samples to generate 3 line-segments.
    if ((m_num_samples - 1) % step == 0) {
        m_view_end += step;
        m_view_start += step;
    }

    // Store stats
    m_graph_stats.data_width = view_width();
    m_graph_stats.start = view_start();
    m_graph_stats.end = view_end();
}

void ViewWindow::update_scroll(const double sample_progress) {
    const int step = calculate_step();

    // sample_progress is a number between 0 and 1 representing how far we are between two sample intervals
    // m_sample_scroll is a number between 0 and step representing how far we are between two step intervals.
    // Skipping this call freezes the auto-scroll; update_offset still applies pan movement.
    const int step_progress = m_num_samples > 0 ? (m_num_samples - 1) % step : 0;
    m_sample_scroll = step_progress + sample_progress;

    update_offset();
}

void ViewWindow::update_offset() {
    const int step = calculate_step();

    // Total offset is the auto-scroll progress plus the fractional pan offset
    double scroll_offset = m_sample_scroll + m_pan_offset;

    // The total scroll must stay within one step; fold any whole steps into the
    // integer view bounds and regenerate the vertices to match
    const int fold_steps = static_cast<int>(std::floor(scroll_offset / step));
    if (fold_steps != 0) {
        m_view_start += fold_steps * step;
        m_view_end   += fold_steps * step;
        m_pan_offset -= fold_steps * step;
        scroll_offset -= fold_steps * step;
    }

    const int data_width = view_width();

    const int visible_count = data_width / step;
    const double graph_screen_width = 2.0 * visible_count / (visible_count - 1);
    m_scroll_offset = scroll_offset * graph_screen_width / data_width;

    m_graph_stats.scroll_offset = get_scroll_offset();
}