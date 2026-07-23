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

class ViewWindow {
public:
    ViewWindow(int core_id,
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
    [[nodiscard]] int num_samples() const { return m_num_samples; }

    void set_num_samples(int n) { m_num_samples = n; }

    [[nodiscard]] int calculate_step() const;

    void add_sample();

    void update_scroll(double sample_progress);

    [[nodiscard]] double get_scroll_offset() const {
        return m_scroll_offset;
    }

private:
    void update_offset();

    int m_core_id;
    int m_max_vertices;
    int m_num_samples = 0;

    int m_view_start = -12;
    int m_view_end   = -2;

    double m_scroll_offset = 0.0;
    double m_pan_offset = 0.0;
    // Auto-scroll progress within the current step, in sample units [0, step)
    double m_sample_scroll = 0.0;
};