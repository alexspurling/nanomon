#include "BetterCpuGraph.h"
#include "CpuGraph.h"

#include <chrono>
#include <iostream>
#include <nanogui/opengl.h>
#include <nanogui/renderpass.h>

using nanogui::Vector3f;

// ---- colour palette ----

static const Vector3f core_colours[] = {
    {0.0f, 1.0f, 0.0f},  // green
    {1.0f, 0.0f, 0.0f},  // red
    {0.0f, 0.5f, 1.0f},  // light blue
    {1.0f, 1.0f, 0.0f},  // yellow
    {1.0f, 0.0f, 1.0f},  // magenta
    {0.0f, 1.0f, 1.0f},  // cyan
    {1.0f, 0.5f, 0.0f},  // orange
    {0.5f, 0.0f, 1.0f},  // purple
};
static constexpr size_t num_colours = std::size(core_colours);
static const Vector3f GRID_COLOUR = {0.0f, 0.4f, 0.0f};

// ---- shared GLSL ----

static const char *VERT_SRC = R"(#version 330
in vec2 points;
uniform float x_offset;
void main() {
    gl_Position = vec4(points.x - x_offset, points.y, 0.0, 1.0);
})";

static const char *FRAG_SRC = R"(#version 330
uniform vec3 line_color;
out vec4 color;
void main() {
    color = vec4(line_color, 1.0);
})";

// ---- helper: upload & draw one line strip ----

static void render_line_strip(nanogui::Shader *shader,
                              const std::vector<float> &verts,
                              const float x_offset,
                              const Vector3f &colour) {
    using namespace nanogui;
    const size_t n = verts.size() / 2;
    shader->set_buffer("points", VariableType::Float32, {n, 2}, verts.data());
    shader->set_uniform("line_color", colour);
    shader->set_uniform("x_offset", x_offset);
    shader->begin();
    shader->draw_array(Shader::PrimitiveType::LineStrip, 0, n, false);
    shader->end();
}

// ============================================================
//  BetterCpuGraph
// ============================================================

BetterCpuGraph::BetterCpuGraph(Widget *parent)
    : Canvas(parent, 1) {

    using namespace nanogui;

    m_shader = new Shader(render_pass(),
                          "better_cpu_graph_shader",
                          VERT_SRC, FRAG_SRC);

    m_grid_shader = new Shader(render_pass(),
                               "better_cpu_graph_grid_shader",
                               VERT_SRC, FRAG_SRC);

    // Discover core count and create one BetterLineGraph per core
    const auto stat = CpuTimesSampler::sample();
    const int num_cores = static_cast<int>(stat.size());

    m_line_graphs.reserve(num_cores);
    for (int core_id = 0; core_id < num_cores; core_id++) {
        m_line_graphs.emplace_back(
            core_id,
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

    m_sample_interval = 60 / SAMPLE_FREQUENCY;
}

// ---- layout ----

void BetterCpuGraph::perform_layout(NVGcontext *ctx) {
    if (parent()) {
        set_position({0, 0});
        set_size(parent()->size());
    }
    Widget::perform_layout(ctx);
}

void BetterCpuGraph::draw(NVGcontext *ctx) {
    Canvas::draw(ctx);
}

// ---- input ----

bool BetterCpuGraph::mouse_button_event(const Vector2i &p, int button,
                                         bool down, int modifiers) {
    if (button == 0) {
        if (down) {
            m_dragging = true;
            m_was_paused_before_drag = m_paused;
            m_paused = true;
        } else if (m_dragging) {
            m_dragging = false;
            m_paused = m_was_paused_before_drag;
        }
    }
    return Canvas::mouse_button_event(p, button, down, modifiers);
}

bool BetterCpuGraph::mouse_drag_event(const Vector2i &p, const Vector2i &rel, const int button, const int modifiers) {
    if (button == 1 && m_dragging && m_size.x() > 0) {
        // Convert pixel delta → NDC delta → sample-index delta
        const double ndc_per_pixel = 2.0 / static_cast<double>(m_size.x());
        const double ndc_delta = -rel.x() * ndc_per_pixel;
        const double idx_delta = ndc_delta * m_window_width / 2.0;

        const int num_samples = m_cpu_history.num_samples();
        for (auto &lg : m_line_graphs) {
            lg.set_num_samples(num_samples);
            lg.pan(idx_delta);
        }
    }
    return Canvas::mouse_drag_event(p, rel, button, modifiers);
}

bool BetterCpuGraph::scroll_event(const Vector2i &p, const Vector2f &rel) {
    if (m_line_graphs.empty() || m_size.x() == 0)
        return Canvas::scroll_event(p, rel);

    // Find the sample index under the mouse cursor
    // const double mouse_ndc = 2.0 * p.x() / static_cast<double>(m_size.x()) - 1.0;
    const double mouse_ratio = p.x() / static_cast<double>(m_size.x());
    constexpr double zoom_factor = 1.1;
    const double factor = (rel.y() < 0) ? zoom_factor : 1.0 / zoom_factor;

    const int num_samples = m_cpu_history.num_samples();
    for (auto &lg : m_line_graphs) {
        lg.set_num_samples(num_samples);
        lg.zoom(factor, mouse_ratio);
    }

    m_window_width = m_line_graphs[0].view_width();
    return Canvas::scroll_event(p, rel);
}

void BetterCpuGraph::nudge_start(const double delta) {
    for (auto &lg : m_line_graphs)
        lg.set_view_start(lg.view_start() + delta);
}

void BetterCpuGraph::nudge_end(const double delta) {
    for (auto &lg : m_line_graphs)
        lg.set_view_end(lg.view_end() + delta);
}

void BetterCpuGraph::draw_grid(const std::vector<float> &vertices) {
    if (vertices.empty())
        return;

    const size_t num_verts = vertices.size() / 2;
    std::vector<float> grid_points;
    grid_points.reserve(num_verts * 4);

    for (size_t i = 0; i < num_verts; ++i) {
        const float x = vertices[i * 2];
        grid_points.push_back(x);       // bottom
        grid_points.push_back(-1.0f);
        grid_points.push_back(x);       // top
        grid_points.push_back(1.0f);
    }

    m_grid_shader->set_buffer("points", VariableType::Float32,
                              {num_verts * 2, 2}, grid_points.data());
    m_grid_shader->set_uniform("x_offset", 0.0f);
    m_grid_shader->set_uniform("line_color", GRID_COLOUR);
    m_grid_shader->begin();
    m_grid_shader->draw_array(Shader::PrimitiveType::Line, 0,
                              num_verts * 2, false);
    m_grid_shader->end();
}

// ---- per-frame update ----

void BetterCpuGraph::draw_contents() {
    using namespace nanogui;

    // ---- 1. sample at fixed interval ----
    if (m_frame_count % m_sample_interval == 0) {
        const int num_samples = m_cpu_history.sample(std::chrono::system_clock::now());

        for (auto &lg : m_line_graphs) {
            lg.set_num_samples(num_samples);
        }

        // ---- 2. auto-scroll (when not paused) ----
        if (!m_paused) {
            for (auto &lg : m_line_graphs) {
                lg.add_sample();
            }
        }
        for (auto &lg : m_line_graphs) {
            lg.update_points();
        }
    }
    for (auto &lg : m_line_graphs) {
        lg.update_scroll(static_cast<double>(m_frame_count % m_sample_interval) / m_sample_interval);
    }

    // ---- 3. generate vertices for the first core (also used for grid) ----
    const auto first_verts = m_line_graphs[0].get_vertices();
    if (first_verts.empty())
        return;

    draw_grid(first_verts);

    // render_line_strip(m_shader, first_verts, core_colours[0 % num_colours]);

    // ---- 4. render each core's line strip ----
    for (size_t i = 0; i < m_line_graphs.size(); ++i) {
        const auto verts = m_line_graphs[i].get_vertices();
        if (verts.empty()) {
            continue;
        }
        const double scroll_offset = m_line_graphs[i].get_scroll_offset();
        // if (i == 0) {
        //     std::cout << "scroll offset: " << scroll_offset << ", frame: " << m_frame_count % m_sample_interval << std::endl;
        // }
        render_line_strip(m_shader, verts, static_cast<float>(scroll_offset), core_colours[i % num_colours]);
    }
    m_frame_count++;
}