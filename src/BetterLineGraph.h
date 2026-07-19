#pragma once

#include <vector>
#include <functional>

/**
 * Statistics about the last vertex generation pass.
 */
struct LineGraphStats {
    int total_samples = 0;
    int excess_samples = 0;
    int step          = 0;
    int count         = 0;
    double data_width = 0.0;
    double start = 0.0;
    double end = 0.0;
    double graph_width = 0.0;
};

/**
 * A windowed view over a sequence of samples identified by index.
 *
 * BetterLineGraph represents a movable, zoomable window defined by
 * floating-point sample indices [m_view_start, m_view_end].  It does
 * not own a fixed-size vertex buffer; instead it generates interleaved
 * (x, y) NDC vertex data on demand by walking the visible portion of
 * the index range.
 *
 * A caller-provided function translates (prev_idx, curr_idx) into a
 * usage value in [0,1], decoupling the vertex generation from the
 * underlying data source.
 */
class BetterLineGraph {
public:
    /**
     * @param core_id            which core's data this graph displays
     * @param sample_fn          function that maps (prev_idx, curr_idx) to a
     *                           usage value in [0, 1]
     * @param sample_window_size number of consecutive samples used to
     *                           compute a single CPU-usage value
     */
    BetterLineGraph(int core_id,
                    std::function<double(int prev_idx, int curr_idx)> sample_fn,
                    int sample_window_size = 2,
                    int max_vertices = 50);

    // ---- window management ----

    /** Reposition the window to exactly [start_idx, end_idx]. */
    // void set_window(double start_idx, double end_idx);

    /** Shift the window by @p delta_idx sample indices. */
    void pan(int num_samples, double delta_idx);

    void set_view_window(int num_samples, double view_start, double view_end);
    void set_view_start(double start);
    void set_view_end(double end);

    void zoom(int num_samples, double factor, double mouse_ratio);

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
    std::vector<float> get_vertices() {
        return m_vertices;
    }

    /** Last-computed stats (set by generate_vertices). */
    [[nodiscard]] const LineGraphStats& last_stats() const {
        return m_last_stats;
    }

    /**
     * Set the window for auto-scroll: anchor the window to @p raw_end
     * (the latest sample index) but snap both start and end to
     * decimation-step boundaries so that the sample indices selected by
     * generate_vertices remain stable as new data arrives.
     *
     * This prevents the "dancing" effect when step > 1.
     */
    void add_sample(int num_samples);

    int calculate_step(int start, int end) const;

    void update_points(int num_samples);

    void update_scroll(int num_samples, double sample_progress);

    double get_scroll_offset() {
        return m_scroll_offset;
    }

private:
    int m_core_id;
    std::function<double(int prev_idx, int curr_idx)> m_sample_fn;
    int m_min_sample_window_size;
    int m_max_vertices;
    std::vector<float> m_vertices;

    int m_view_start = -12;
    int m_view_end   = -2;

    LineGraphStats m_last_stats;
    double m_scroll_offset = 0.0;
};