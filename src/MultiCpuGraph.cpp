#include "MultiCpuGraph.h"
#include "CpuTimesSampler.h"

MultiCpuGraph::MultiCpuGraph(Widget *parent)
    : CpuGraph(parent) {

    // Discover core count and create one VertexGenerator per core
    const auto stat = CpuTimesSampler::sample();
    const int num_cores = static_cast<int>(stat.size());

    m_vertex_generators.reserve(num_cores);
    for (int core_id = 0; core_id < num_cores; core_id++) {
        m_vertex_generators.emplace_back(
            [this, core_id](const int prev_idx, const int curr_idx) -> double {
                const CoreSample& prev = m_cpu_history.sample_at(core_id, prev_idx);
                const CoreSample& curr = m_cpu_history.sample_at(core_id, curr_idx);
                const double total_diff = curr.total_time - prev.total_time;
                const double idle_diff  = curr.idle_time  - prev.idle_time;
                return (total_diff > 0.0)
                    ? (total_diff - idle_diff) / total_diff
                    : 0.0;
            },
            SAMPLE_WINDOW_SIZE,
            MAX_VERTICES);
    }
}

void MultiCpuGraph::draw_graph_content() {
    const int num_samples = m_cpu_history.num_samples();

    // ---- generate vertices for the first core (also used for grid) ----
    auto first_verts = m_vertex_generators[0].generate_vertices(m_view_window, num_samples);
    m_view_window.m_graph_stats.vertex_count = static_cast<int>(first_verts.size() / 2);
    if (first_verts.empty())
        return;

    draw_grid(first_verts);

    // ---- render each core's line strip ----
    for (size_t i = 0; i < m_vertex_generators.size(); ++i) {
        std::vector<float> verts = m_vertex_generators[i].generate_vertices(m_view_window, num_samples);
        if (verts.empty()) {
            continue;
        }
        const double scroll_offset = m_view_window.get_scroll_offset();
        render_line_strip(m_shader, verts, static_cast<float>(scroll_offset), CORE_COLOURS[i % NUM_COLOURS]);
    }
}