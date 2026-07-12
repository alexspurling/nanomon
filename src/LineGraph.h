#ifndef NANOMON_LINEGRAPH_H
#define NANOMON_LINEGRAPH_H

#include <vector>
#include <cstddef>
#include <functional>

class LineGraph {
public:

    explicit LineGraph(size_t num_points,
                     std::function<double(double)> value_func);

    /**
     * Advance the simulation to the given game_time. Recomputes y-values
     * in the buffer if the quantized scroll offset has changed since the
     * last call.
     */
    void advance_time(double game_time);

    /**
     * Force a full recomputation of all y-values in the buffer.
     * Used after dragging shifts the visible window.
     */
    void recompute_y_values();

    /**
     * The smooth sub-step scroll offset in screen (NDC) units,
     * used by the shader for continuous scrolling between quantized steps.
     */
    double get_smooth_scrolling_x_offset() const;

    /**
     * The data-space x value at buffer index i, accounting for the
     * current quantized offset and the visible window.
     */
    double get_sample_x(int i) const;

    /**
     * Total scroll distance in data-space units since the simulation began.
     */
    double get_total_scroll() const;

    /** Raw access to the interleaved x/y buffer for rendering. */
    const float* data() const { return m_data.data(); }

    /** Number of points in the buffer. */
    size_t size() const;

    double start_x() const { return m_start_x; }
    double end_x() const { return m_end_x; }
    double get_data_width() const { return m_end_x - m_start_x; }
    void set_start_x(double x) { m_start_x = x; }
    void set_end_x(double x) { m_end_x = x; }

    double scroll_speed() const { return m_scroll_speed; }
    void set_scroll_speed(double speed) { m_scroll_speed = speed; }

    double x_offset() const { return m_x_offset; }

    size_t num_points() const { return m_num_points; }

    double compute_y(double sample_x) const;

    /**
     * Process a drag offset in NDC units. If the accumulated data-space
     * delta exceeds one grid spacing, shifts the view window by whole
     * grid spacings and recomputes y-values. Returns the adjusted drag
     * offset (remainder less than one grid spacing).
     */
    double drag_to_offset(double drag_offset);

private:
    void initialise_x_values();

    std::function<double(double)> m_value_func;
    std::vector<float> m_data;
    double m_x_offset = 0.0;
    double m_scroll_speed = 1.0;
    double m_start_x = 0.0;
    double m_end_x = 10.0;
    double m_game_time = 0.0;
    size_t m_num_points;
};

#endif // NANOMON_LINEGRAPH_H