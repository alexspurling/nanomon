#include "BetterLineGraph.h"
#include <algorithm>
#include <cmath>

BetterLineGraph::BetterLineGraph(const int core_id, const int sample_window_size)
    : m_core_id(core_id), m_sample_window_size(sample_window_size) {}

void BetterLineGraph::pan(const double delta_idx) {
    m_view_start += delta_idx;
    m_view_end   += delta_idx;
}

void BetterLineGraph::zoom(const double factor, const double center_idx) {
    const double half_width = view_width() * 0.5 / factor;
    m_view_start = center_idx - half_width;
    m_view_end   = center_idx + half_width;
}

void BetterLineGraph::set_window(const double start_idx, const double end_idx) {
    m_view_start = start_idx;
    m_view_end   = end_idx;
}

void BetterLineGraph::set_window_auto_scroll(const int num_samples, const int max_vertices) {

    if (num_samples < m_sample_window_size + 1) {
        m_view_end   = num_samples - 1;
        m_view_start = num_samples - 1 - view_width();
        return;
    }

    const int raw_end = num_samples - 1;

    // Compute the decimation step the same way generate_vertices does
    const int first = std::max(static_cast<int>(std::floor(m_view_start)), m_sample_window_size);
    const int last = std::min(static_cast<int>(std::ceil(m_view_end)), num_samples - 1);

    const int visible_count = last - first + 1;
    const int step = std::max(1, visible_count / max_vertices);

    // Snap end_idx down to the nearest step boundary, then derive
    // start_idx such that the window maintains its current width.
    const double end_idx   = std::floor(raw_end / step) * step;
    const double start_idx = end_idx - std::floor(view_width() / step) * step;

    m_view_start = start_idx;
    m_view_end   = end_idx;
}

std::vector<float> BetterLineGraph::generate_vertices(
        const CpuHistory& history, const int max_vertices) {

    const int total_samples = history.num_samples();
    if (total_samples < m_sample_window_size + 1) {
        m_last_stats = {};
        return {};
    }

    // Clamp the visible range to available data
    const int first = std::max(static_cast<int>(std::floor(m_view_start)), m_sample_window_size);
    const int last = std::min(static_cast<int>(std::ceil(m_view_end)), total_samples - 1);

    if (first > last) {
        m_last_stats = {};
        return {};
    }

    const int visible_count = last - first + 1;

    // Decimation: if the window covers more samples than max_vertices,
    // take every Nth sample so the GPU never gets overwhelmed.
    const int step = std::max(1, visible_count / max_vertices);
    const int count = (visible_count + step - 1) / step; // ceil division

    const double data_width = m_view_end - m_view_start;

    // Store stats
    m_last_stats.total_samples = total_samples;
    m_last_stats.visible_count = visible_count;
    m_last_stats.step          = step;
    m_last_stats.count         = count;
    m_last_stats.data_width    = data_width;

    std::vector<float> vertices;
    vertices.reserve(count * 2);

    for (int i = 0; i < count; ++i) {
        const int sample_idx = first + i * step;

        // Map sample index → NDC x in [-1, 1]
        const double t = (static_cast<double>(sample_idx) - m_view_start) / data_width;
        const float x = static_cast<float>(t * 2.0 - 1.0);

        // Compute CPU usage for this sample
        const CoreSample prev = history.sample_at(m_core_id, sample_idx - m_sample_window_size * step);
        const CoreSample curr = history.sample_at(m_core_id, sample_idx);

        const double total_diff = curr.total_time - prev.total_time;
        const double idle_diff  = curr.idle_time  - prev.idle_time;

        float usage = 0.0f;
        if (total_diff > 0.0) {
            usage = static_cast<float>((total_diff - idle_diff) / total_diff);
        }

        // Map usage [0, 1] → NDC y [-1, 1]
        const float y = usage * 2.0f - 1.0f;

        vertices.push_back(x);
        vertices.push_back(y);
    }

    return vertices;
}