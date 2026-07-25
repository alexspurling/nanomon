#include "Graph.h"

#include <nanogui/renderpass.h>

using nanogui::Vector3f;

const Vector3f Graph::GRID_COLOUR = {0.05f, 0.15f, 0.05f};

// ---- GLSL ----

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

// ============================================================
//  Graph
// ============================================================

Graph::Graph(Widget *parent, DataSource *data_source)
    : Canvas(parent, 1, true), m_data_source(data_source) {

    using namespace nanogui;

    m_shader = new Shader(render_pass(),
                          "graph_line_shader",
                          VERT_SRC, FRAG_SRC);

    m_grid_shader = new Shader(render_pass(),
                               "graph_grid_shader",
                               VERT_SRC, FRAG_SRC);

    render_pass()->set_depth_test(RenderPass::DepthTest::LessEqual, false);

    m_sample_interval = 60 / SAMPLE_FREQUENCY;

    if (m_data_source) {
        rebuild_vertex_generators();
    }
}

void Graph::set_data_source(DataSource *source) {
    m_data_source = source;
    m_view_window = ViewWindow{MAX_VERTICES};
    m_frame_count = 0;
    if (m_data_source) {
        rebuild_vertex_generators();
    }
}

void Graph::rebuild_vertex_generators() {
    m_vertex_generators.clear();
    const int n = m_data_source->num_series();
    m_vertex_generators.reserve(n);

    for (int i = 0; i < n; ++i) {
        const auto &cfg = m_data_source->series_config(i);
        m_vertex_generators.emplace_back(
            [this, i](int prev_idx, int curr_idx) -> double {
                return m_data_source->compute_value(i, prev_idx, curr_idx);
            },
            SAMPLE_WINDOW_SIZE,
            MAX_VERTICES,
            cfg.y_min,
            cfg.y_max);
    }
}

// ---- layout ----

void Graph::perform_layout(NVGcontext *ctx) {
    if (parent()) {
        set_position({0, 0});
        set_size(parent()->size());
    }
    Canvas::perform_layout(ctx);
}

void Graph::draw(NVGcontext *ctx) {
    Canvas::draw(ctx);
}

// ---- input ----

bool Graph::mouse_button_event(const Vector2i &p, int button,
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

bool Graph::mouse_drag_event(const Vector2i &p, const Vector2i &rel,
                              int button, int modifiers) {
    if (button == 1 && m_dragging && m_size.x() > 0 && m_data_source) {
        const double ndc_per_pixel = 2.0 / static_cast<double>(m_size.x());
        const double ndc_delta = -rel.x() * ndc_per_pixel;
        const double idx_delta = ndc_delta * m_view_window.view_width() / 2.0;

        m_view_window.set_num_samples(m_data_source->num_samples());
        m_view_window.pan(idx_delta);
    }
    return Canvas::mouse_drag_event(p, rel, button, modifiers);
}

bool Graph::scroll_event(const Vector2i &p, const Vector2f &rel) {
    if (m_size.x() == 0 || !m_data_source)
        return Canvas::scroll_event(p, rel);

    const double mouse_ratio = p.x() / static_cast<double>(m_size.x());
    constexpr double zoom_factor = 1.1;
    const double factor = (rel.y() < 0) ? zoom_factor : 1.0 / zoom_factor;

    m_view_window.set_num_samples(m_data_source->num_samples());
    m_view_window.zoom(factor, mouse_ratio);

    return Canvas::scroll_event(p, rel);
}

void Graph::nudge_start(double delta) {
    m_view_window.set_view_start(m_view_window.view_start() + static_cast<int>(delta));
}

void Graph::nudge_end(double delta) {
    m_view_window.set_view_end(m_view_window.view_end() + static_cast<int>(delta));
}

// ---- helpers ----

void Graph::draw_grid(const std::vector<float> &vertices) {
    if (vertices.empty())
        return;

    const size_t num_verts = vertices.size() / 2;
    std::vector<float> grid_points;
    grid_points.reserve(num_verts * 4);

    for (size_t i = 0; i < num_verts; ++i) {
        const float x = vertices[i * 2];
        grid_points.push_back(x);
        grid_points.push_back(-1.0f);
        grid_points.push_back(x);
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

void Graph::render_line_strip(Shader *shader,
                               const std::vector<float> &verts,
                               float x_offset,
                               const Vector3f &colour) {
    using namespace nanogui;
    if (verts.empty())
        return;

    const size_t num_verts = verts.size() / 2;
    shader->set_buffer("points", VariableType::Float32, {num_verts, 2}, verts.data());
    shader->set_uniform("line_color", colour);
    shader->set_uniform("x_offset", x_offset);
    shader->begin();
    shader->draw_array(Shader::PrimitiveType::LineStrip, 0, num_verts, false);
    shader->end();
}

// ---- per-frame update ----

void Graph::draw_contents() {
    using namespace nanogui;

    if (!m_data_source)
        return;

    // ---- 1. sample at fixed interval ----
    if (m_frame_count % m_sample_interval == 0) {
        m_data_source->sample();
        const int n_samples = m_data_source->num_samples();

        m_view_window.set_num_samples(n_samples);

        if (!m_paused) {
            m_view_window.add_sample();
        }
    }
    if (!m_paused) {
        m_view_window.update_scroll(
            static_cast<double>(m_frame_count % m_sample_interval) / m_sample_interval);
    }

    // ---- 2. draw all series ----
    const int n_samples = m_data_source->num_samples();

    // Generate vertices for the first series (used for grid)
    if (m_vertex_generators.empty())
        return;

    auto &first_gen = m_vertex_generators[0];
    // Re-apply y-range in case config was updated
    if (m_data_source->num_series() > 0) {
        const auto &cfg = m_data_source->series_config(0);
        first_gen.set_y_range(cfg.y_min, cfg.y_max);
    }
    auto first_verts = first_gen.generate_vertices(m_view_window, n_samples);
    m_view_window.m_graph_stats.vertex_count = static_cast<int>(first_verts.size() / 2);

    if (first_verts.empty())
        return;

    draw_grid(first_verts);

    // Render each series' line strip
    for (size_t i = 0; i < m_vertex_generators.size(); ++i) {
        const auto &cfg = m_data_source->series_config(static_cast<int>(i));
        m_vertex_generators[i].set_y_range(cfg.y_min, cfg.y_max);

        auto verts = m_vertex_generators[i].generate_vertices(m_view_window, n_samples);
        if (verts.empty())
            continue;

        const float scroll_offset = static_cast<float>(m_view_window.get_scroll_offset());
        render_line_strip(m_shader, verts, scroll_offset, cfg.color);
    }

    m_frame_count++;
}