#ifndef NANOMON_LINEGRAPH_H
#define NANOMON_LINEGRAPH_H

#include <vector>
#include <cstddef>
#include <functional>

class LineGraph {
public:

    explicit LineGraph(size_t num_points, std::function<double(double)> value_func);

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
     * Combined x offset in NDC units: smooth sub-step scrolling offset
     * plus any drag offset. Used by the shader for continuous scrolling.
     */
    [[nodiscard]]
    double get_x_offset() const;

    /**
     * The data-space x value at buffer index i, accounting for the
     * current quantized offset and the visible window.
     */
    [[nodiscard]]
    double get_sample_x(size_t i) const;

    /** Raw access to the interleaved x/y buffer for rendering. */
    [[nodiscard]]
    const float* data() const { return m_data.data(); }

    /** Number of points in the buffer. */
    [[nodiscard]]
    size_t size() const;

    [[nodiscard]]
    double start_x() const { return m_start_x; }
    [[nodiscard]]
    double end_x() const { return m_end_x; }
    [[nodiscard]]
    double get_data_width() const { return m_end_x - m_start_x; }
    void set_start_and_end_x(double start, double end);

    [[nodiscard]]
    double scroll_speed() const { return m_scroll_speed; }
    void set_scroll_speed(double speed) { m_scroll_speed = speed; }

    /**
     * The smooth scrolling remainder offset, always less than one grid
     * spacing (dx_data). This is the sub-grid portion of the total
     * scroll that hasn't yet been quantized into m_start_x/m_end_x.
     */
    [[nodiscard]]
    double x_offset() const { return m_smooth_offset; }

    [[nodiscard]]
    size_t num_points() const { return m_num_points; }

    [[nodiscard]]
    double compute_y(double sample_x) const;

    /**
     * Apply an incremental drag delta (in NDC units). Accumulates the
     * offset internally. If the accumulated data-space delta exceeds
     * one grid spacing, shifts the view window by whole grid spacings
     * and recomputes y-values, keeping only the sub-grid remainder.
     */
    void apply_drag_offset(double delta_ndc);

private:
    void initialise_x_values();

    std::function<double(double)> m_value_func;
    std::vector<float> m_data;
    double m_smooth_offset = 0.0;
    double m_last_quantized = 0.0;
    double m_scroll_speed = 6.0;
    double m_start_x = 0.0;
    double m_end_x = 10.0;
    double m_game_time = 0.0;
    double m_drag_offset = 0.0;
    size_t m_num_points;
};

#endif // NANOMON_LINEGRAPH_H