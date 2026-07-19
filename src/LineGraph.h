#pragma once

#include <vector>
#include <functional>

struct LineGraphStats {
    int total_samples = 0;
    int excess_samples = 0;
    int step          = 0;
    int count         = 0;
    double data_width = 0.0;
    double start = 0.0;
    double end = 0.0;
    double scroll_offset = 0.0;
};

class LineGraph {
public:
    LineGraph(int core_id,
                    std::function<double(int prev_idx, int curr_idx)> sample_fn,
                    int sample_window_size = 2,
                    int max_vertices = 50);

    // ---- window management ----

    /** Shift the window by @p delta_idx sample indices. */
    void pan(double delta_idx);

    void set_view_window(double view_start, double view_end);
    void set_view_start(int start);
    void set_view_end(int end);

    void zoom(double factor, double mouse_ratio);

    // ---- accessors ----

    [[nodiscard]] int view_start() const { return m_view_start; }
    [[nodiscard]] int view_end()   const { return m_view_end; }
    [[nodiscard]] int view_width() const { return m_view_end - m_view_start; }

    void set_num_samples(int n) { m_num_samples = n; }

    // ---- vertex generation ----

    std::vector<float> get_vertices() {
        return m_vertices;
    }

    [[nodiscard]] const LineGraphStats& last_stats() const {
        return m_last_stats;
    }

    void add_sample();

    [[nodiscard]] int calculate_step(int start, int end) const;

    void update_points();

    void update_scroll(double sample_progress);

    [[nodiscard]] double get_scroll_offset() const {
        return m_scroll_offset;
    }

private:
    void update_offset();

    int m_core_id;
    std::function<double(int prev_idx, int curr_idx)> m_sample_fn;
    int m_min_sample_window_size;
    int m_max_vertices;
    int m_num_samples = 0;
    std::vector<float> m_vertices;

    int m_view_start = -12;
    int m_view_end   = -2;

    LineGraphStats m_last_stats;
    double m_scroll_offset = 0.0;
    double m_pan_offset = 0.0;
    // Auto-scroll progress within the current step, in sample units [0, step)
    double m_sample_scroll = 0.0;
};