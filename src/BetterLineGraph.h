#pragma once

#include <vector>
#include "CpuHistory.h"

/**
 * Statistics about the last vertex generation pass.
 */
struct LineGraphStats {
    int total_samples = 0;
    int visible_count = 0;
    int step          = 0;
    int count         = 0;
    double data_width = 0.0;
};

/**
 * A windowed view onto a single core's CPU history data.
 *
 * BetterLineGraph represents a movable, zoomable window defined by
 * floating-point sample indices [m_view_start, m_view_end].  It does
 * not own a fixed-size vertex buffer; instead it generates interleaved
 * (x, y) NDC vertex data on demand by walking the visible portion of
 * the underlying CpuHistory.
 *
 * This keeps the design simple: the window is the single source of
 * truth, and vertex data is always consistent with the current window
 * and the current history state.
 */
class BetterLineGraph {
public:
    /**
     * @param core_id           which core's data this graph displays
     * @param sample_window_size number of consecutive samples used to
     *                           compute a single CPU-usage value
     */
    BetterLineGraph(int core_id, int sample_window_size = 2);

    // ---- window management ----

    /** Reposition the window to exactly [start_idx, end_idx]. */
    void set_window(double start_idx, double end_idx);

    /** Shift the window by @p delta_idx sample indices. */
    void pan(double delta_idx);

    /**
     * Zoom the window by @p factor around @p center_idx (in sample-index
     * space).  factor > 1  → zoom in  (narrower window);
     * factor < 1  → zoom out (wider window).
     */
    void zoom(double factor, double center_idx);

    // ---- accessors ----

    double view_start() const { return m_view_start; }
    double view_end()   const { return m_view_end; }
    double view_width() const { return m_view_end - m_view_start; }

    // ---- vertex generation ----

    /**
     * Produce interleaved (x, y) vertex data in NDC space [-1, 1] for
     * every *visible* sample in the current window.
     *
     * If the window covers more samples than @p max_vertices, the output
     * is decimated (every Nth sample) so the GPU never receives more
     * vertices than necessary.
     *
     * @return  flat vector of floats: [x0, y0, x1, y1, …]
     *          empty when there is not enough history data yet.
     */
    std::vector<float> generate_vertices(const CpuHistory& history,
                                         int max_vertices);

    /** Last-computed stats (set by generate_vertices). */
    [[nodiscard]] const LineGraphStats& last_stats() const {
        return m_last_stats;
    }

private:
    int m_core_id;
    int m_sample_window_size;
    double m_view_start = 0.0;
    double m_view_end   = 30.0;

    LineGraphStats m_last_stats;
};