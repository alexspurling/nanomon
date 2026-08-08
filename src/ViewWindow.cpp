#include "ViewWindow.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <ostream>

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

void ViewWindow::set_view_window(double view_start, double view_end) {
    // The view bounds are stored as integers; the fractional remainder of the
    // start position is kept in m_pan_offset (always in [0, 1)) and applied as
    // a sub-sample x offset by update_scroll.

    // The step depends on the width of the *new* window, not the one it replaces
    const int width = std::max(1, static_cast<int>(std::lround(view_end - view_start)));
    const int step = std::max(1, width / m_max_vertices);

    // add_sample() advances the window by a whole step at a time, so the right
    // edge has to sit on the step grid at or behind the newest sample. Any
    // further right and the next advance carries the window past the live data.
    const int max_view_end = std::max(0, num_samples() - 1) / step * step;

    // The integer left edge is confined to [lo, hi]:
    //   hi = max_view_end - width : flush against the newest sample (right cap)
    //   lo = min(0, hi)           : flush against the first sample, so the view
    //                               cannot scroll left past sample 0 into a gap.
    // When the window is wider than the available data the two caps meet
    // (lo == hi) and the window locks to the right with the gap on the left.
    const int hi = max_view_end - width;
    const int lo = std::min(0, hi);

    if (view_start > hi) {
        // Clamped right: snap flush to the live edge. The sub-sample position is
        // then supplied entirely by the auto-scroll, so the pan offset is spent.
        m_view_start = hi;
        m_view_end = max_view_end;
        m_pan_offset = 0.0;
    } else if (view_start < lo) {
        // Clamped left: pin the left edge to the first sample. A gap may remain
        // to its left but the view cannot scroll further into it.
        m_view_start = lo;
        m_view_end = lo + width;
        m_pan_offset = 0.0;
    } else {
        const double start_floor = std::floor(view_start);
        m_view_start = static_cast<int>(start_floor);
        m_pan_offset = view_start - start_floor;
        // Preserve the requested width rather than flooring both ends independently
        m_view_end = m_view_start + width;
    }

    // m_sample_scroll is expressed in units of the old step; re-derive it for the
    // new one so that it stays inside [0, step) and update_offset() does not fold
    // a phantom step into the view bounds.
    const double sample_progress = m_sample_scroll - std::floor(m_sample_scroll);
    m_sample_scroll = std::max(0, num_samples() - 1) % step + sample_progress;

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
    m_graph_stats.step = step;

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
    const int step_progress = (m_num_samples - 1) % step;
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
        const int prev_view_start = m_view_start;
        const int prev_view_end = m_view_end;
        const double prev_pan_offset = m_pan_offset;
        const double prev_scroll_offset = scroll_offset;
        m_view_start += fold_steps * step;
        m_view_end   += fold_steps * step;
        m_pan_offset -= fold_steps * step;
        scroll_offset -= fold_steps * step;
        // std::cout << "view_start: " << prev_view_start << " -> " << m_view_start <<
        //    ", m_view_end: " << prev_view_end << " -> " << m_view_end <<
        //    ", m_pan_offset: " << prev_pan_offset << " -> " << m_pan_offset <<
        //    ", scroll_offset: " << prev_scroll_offset << " -> " << scroll_offset <<
        //    ", num_samples: " << prev_scroll_offset << " -> " << scroll_offset <<
        //        std::endl;
    }

    const int data_width = view_width();

    const int visible_count = data_width / step;
    const double graph_screen_width = 2.0 * visible_count / (visible_count - 1);
    m_scroll_offset = scroll_offset * graph_screen_width / data_width;

    m_graph_stats.scroll_offset = scroll_offset;
    m_graph_stats.auto_scroll_offset = m_sample_scroll;
    m_graph_stats.pan_offset = m_pan_offset;
    m_graph_stats.excess_samples = m_num_samples - m_view_end;
}