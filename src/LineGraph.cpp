#include "LineGraph.h"

#include <cmath>
#include <iostream>

LineGraph::LineGraph(const size_t num_points, std::function<double(double)> value_func)
    : m_value_func(std::move(value_func)), m_num_points(num_points) {
    m_data.resize(m_num_points * 2, 0.0f);
    initialise_x_values();
}


size_t LineGraph::size() const {
    return m_num_points;
}

void LineGraph::initialise_x_values() {
    const float dx = 2.0f / static_cast<float>(m_num_points - 2);
    for (size_t i = 0; i < m_num_points; i++) {
        const float x = -1.0f + static_cast<float>(i) * dx;
        m_data[i * 2] = x;
    }
}

void LineGraph::advance_time(const double game_time) {
    m_game_time = game_time;

    const double dx_data = get_data_width() / (static_cast<double>(m_num_points) - 2.0);
    const double total_scroll = m_scroll_speed * m_game_time;
    const double quantized_offset = std::floor(total_scroll / dx_data) * dx_data;

    if (quantized_offset != m_last_quantized) {
        const double delta = quantized_offset - m_last_quantized;
        m_last_quantized = quantized_offset;
        m_start_x += delta;
        m_end_x += delta;
        recompute_y_values();
    }

    // Smooth scrolling offset: sub-grid remainder in NDC units
    const double remainder_data = total_scroll - m_last_quantized;
    m_smooth_offset = remainder_data * 2.0 / get_data_width();
    // std::cout << "total scroll: " << total_scroll << ", new smooth offset " << m_smooth_offset << std::endl;
}

void LineGraph::recompute_y_values() {
    for (size_t i = 0; i < m_num_points; i++) {
        const double sample_x = get_sample_x(i);
        const double y = compute_y(sample_x);
        m_data[i * 2 + 1] = static_cast<float>(y);
    }
}

double LineGraph::get_x_offset() const {
    return m_smooth_offset + m_drag_offset;
}

double LineGraph::get_sample_x(const size_t i) const {
    const double t = static_cast<double>(i) / (static_cast<double>(m_num_points) - 2.0);
    return m_start_x + t * get_data_width();
}

void LineGraph::set_start_and_end_x(const double start, const double end) {
    const double total_scroll = m_scroll_speed * m_game_time - 2.0;
    double adjusted_end = end;
    double adjusted_start = start;
    if (adjusted_end > total_scroll) {
        const double delta = adjusted_end - total_scroll;
        adjusted_end = total_scroll;
        adjusted_start -= delta;
    }
    m_start_x = adjusted_start;
    m_end_x = adjusted_end;
}

double LineGraph::compute_y(const double sample_x) const {
    // value_func returns in range 0.0..1.0, convert to -1.0..1.0
    return m_value_func(sample_x) * 2.0 - 1.0;
}

void LineGraph::apply_drag_offset(const double delta_ndc) {
    m_drag_offset += delta_ndc;

    // One grid spacing in data-space and NDC units
    const double dx_data = get_data_width() / (static_cast<double>(m_num_points) - 2.0);
    const double dx_ndc = dx_data * 2.0 / get_data_width();

    // Snap to grid when accumulated drag offset exceeds one grid spacing
    if (m_drag_offset >= dx_ndc) {
        const int num_steps = static_cast<int>(m_drag_offset / dx_ndc);
        const double shift = num_steps * dx_data;
        m_start_x += shift;
        m_end_x += shift;
        m_drag_offset -= num_steps * dx_ndc;
        recompute_y_values();
    } else if (m_drag_offset <= -dx_ndc) {
        const int num_steps = static_cast<int>((-m_drag_offset) / dx_ndc);
        const double shift = num_steps * dx_data;
        m_start_x -= shift;
        m_end_x -= shift;
        m_drag_offset += num_steps * dx_ndc;
        recompute_y_values();
    }

    // Cap: rightmost visible point shouldn't exceed total_scroll.
    // Rightmost visible data x = m_end_x + (m_smooth_offset + m_drag_offset) * data_width / 2
    const double total_scroll = m_scroll_speed * m_game_time - 2.0;
    const double max_drag_offset = (total_scroll - m_end_x) * 2.0 / get_data_width() - m_smooth_offset;
    if (m_drag_offset > max_drag_offset) {
        m_drag_offset = max_drag_offset;
    }
}

