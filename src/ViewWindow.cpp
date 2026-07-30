#include "ViewWindow.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <ostream>

namespace {
// Written next to the working directory of the running app
constexpr const char *RECORDING_PATH = "viewwindow-recording.txt";
}

ViewWindow::ViewWindow(const int max_vertices)
    : m_max_vertices(max_vertices) {
    // The window width is the only construction input a replay needs
    const RecordScope scope(*this, ViewWindowOp::Construct, max_vertices);
}

void ViewWindow::pan(const double delta_idx) {
    const RecordScope scope(*this, ViewWindowOp::Pan, delta_idx);

    // Pan relative to the true fractional position so sub-sample deltas accumulate
    const double view_start = m_view_start + m_pan_offset;
    const double view_end = m_view_end + m_pan_offset;
    set_view_window(view_start + delta_idx, view_end + delta_idx);
    update_offset();
}

void ViewWindow::zoom(const double factor, const double mouse_ratio) {
    const RecordScope scope(*this, ViewWindowOp::Zoom, factor, mouse_ratio);

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
    const RecordScope scope(*this, ViewWindowOp::SetViewWindow, view_start, view_end);

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

    // The right edge actually lands at view_start + width, so test that rather
    // than the requested view_end: rounding the width to a whole number of
    // samples can otherwise nudge the edge past the cap.
    if (view_start + width > max_view_end) {
        // Snap flush to the live edge. The sub-sample position is then supplied
        // entirely by the auto-scroll, so the pan offset is spent.
        m_view_end = max_view_end;
        m_view_start = max_view_end - width;
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
    const RecordScope scope(*this, ViewWindowOp::SetViewStart, start);
    m_view_start = start;
}

void ViewWindow::set_view_end(const int end) {
    const RecordScope scope(*this, ViewWindowOp::SetViewEnd, end);
    m_view_end = end;
}

void ViewWindow::set_num_samples(const int n) {
    const RecordScope scope(*this, ViewWindowOp::SetNumSamples, n);
    m_num_samples = n;
    m_graph_stats.total_samples = m_num_samples;
}

int ViewWindow::calculate_step() const {
    return std::max(1, view_width() / m_max_vertices);
}

void ViewWindow::add_sample() {
    const RecordScope scope(*this, ViewWindowOp::AddSample);

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
    const RecordScope scope(*this, ViewWindowOp::UpdateScroll, sample_progress);

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

    // The window is only ever allowed to reach the newest sample, never past it,
    // so anything at or below zero means a pan/zoom has escaped its bounds
    if (m_export_on_violation && m_graph_stats.excess_samples <= 0) {
        export_recording_and_exit("excess_samples <= 0");
    }
}

ViewWindowState ViewWindow::state() const {
    ViewWindowState s;
    s.view_start = m_view_start;
    s.view_end = m_view_end;
    s.num_samples = m_num_samples;
    s.pan_offset = m_pan_offset;
    s.sample_scroll = m_sample_scroll;
    s.scroll_offset = m_scroll_offset;
    s.stats_scroll_offset = m_graph_stats.scroll_offset;
    s.excess_samples = m_graph_stats.excess_samples;
    s.step = m_graph_stats.step;
    s.data_width = m_graph_stats.data_width;
    return s;
}

void ViewWindow::export_recording_and_exit(const char *reason) {
    // We are inside the failing call, so its event is already the last one in the
    // recording; fill in the state it reached before writing the file
    m_recorder.update_last_state(state());
    const bool saved = m_recorder.save(RECORDING_PATH);

    std::cerr << "ViewWindow bounds violated (" << reason << "): "
              << "view_start=" << m_view_start << " view_end=" << m_view_end
              << " num_samples=" << m_num_samples << " step=" << calculate_step()
              << " pan_offset=" << m_pan_offset << " sample_scroll=" << m_sample_scroll << "\n"
              << (saved ? "Wrote " : "FAILED to write ") << RECORDING_PATH
              << " (" << m_recorder.size() << " events). Replay it with:\n"
              << "  ./viewwindow_replay " << RECORDING_PATH << std::endl;

    std::exit(EXIT_FAILURE);
}
