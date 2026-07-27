#include "Graph.h"

#include <nanogui/renderpass.h>
#include <nanogui/button.h>
#include <nanogui/layout.h>
#include <nanogui/screen.h>

#include "nanogui/popup.h"

using namespace nanogui;

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

Graph::Graph(Widget *parent, std::unique_ptr<DataSource> data_source, StatsWidget *stats_widget)
    : Canvas(parent, 1, true), m_data_source(std::move(data_source)), m_stats_widget(stats_widget) {

    m_shader = new Shader(render_pass(), "graph_line_shader", VERT_SRC, FRAG_SRC);
    m_grid_shader = new Shader(render_pass(), "graph_grid_shader", VERT_SRC, FRAG_SRC);

    render_pass()->set_depth_test(RenderPass::DepthTest::LessEqual, false);

    m_sample_interval = 60 / SAMPLE_FREQUENCY;

    const int n = m_data_source->num_series();
    m_vertex_generators.reserve(n);

    for (int i = 0; i < n; ++i) {
        const auto &cfg = m_data_source->series_config(i);
        m_vertex_generators.emplace_back(
            [this, i](const int prev_idx, const int curr_idx) -> double {
                return m_data_source->compute_value(i, prev_idx, curr_idx);
            },
            SAMPLE_WINDOW_SIZE,
            MAX_VERTICES,
            cfg.y_min,
            cfg.y_max);
    }

    create_context_menu();
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

void Graph::create_context_menu() {

    m_context_menu = new Popup(screen(), window());
    m_context_menu->set_size(Vector2i(200, 100));
    m_context_menu->set_position(Vector2i(50, 50));
    m_context_menu->set_visible(false);

    auto *popup_content = new Widget(m_context_menu);
    popup_content->set_layout(new GroupLayout());

    auto *pause_btn = new Button(popup_content, "Pause");
    pause_btn->set_callback([this, pause_btn] {
        m_paused = !m_paused;
        pause_btn->set_caption(m_paused ? "Resume" : "Pause");
        m_context_menu->set_visible(false);
    });

    auto *debug_btn = new Button(popup_content, "Debug stats");
    debug_btn->set_callback([this] {
        m_context_menu->set_visible(false);
        m_stats_widget->set_graph(this);
        m_stats_widget->set_visible(!m_stats_widget->visible());
    });
}

bool Graph::mouse_button_event(const Vector2i &p, int button,
                                bool down, int modifiers) {
    if (button == 0) {
        if (down) {
            m_dragging = true;
            m_was_paused_before_drag = m_paused;
            m_paused = true;

            // Dismiss context menu on left-click
            m_context_menu->set_visible(false);
        } else if (m_dragging) {
            m_dragging = false;
            m_paused = m_was_paused_before_drag;
        }
    }

    // Right-click down → show context menu
    if (button == 1 && down) {
        Vector2i screen_pos = absolute_position() + p + Vector2i(18, -29);
        m_context_menu->set_position(screen_pos);
        m_context_menu->set_visible(true);
        m_context_menu->perform_layout(screen()->nvg_context());
        return true;
    }

    return Canvas::mouse_button_event(p, button, down, modifiers);
}

bool Graph::mouse_drag_event(const Vector2i &p, const Vector2i &rel, const int button, const int modifiers) {
    if (button == 1 && m_dragging && m_size.x() > 0) {
        const double ndc_per_pixel = 2.0 / static_cast<double>(m_size.x());
        const double ndc_delta = -rel.x() * ndc_per_pixel;
        const double idx_delta = ndc_delta * m_view_window.view_width() / 2.0;

        m_view_window.pan(idx_delta);
    }
    return Canvas::mouse_drag_event(p, rel, button, modifiers);
}

bool Graph::scroll_event(const Vector2i &p, const Vector2f &rel) {
    const double mouse_ratio = p.x() / static_cast<double>(m_size.x());
    constexpr double zoom_factor = 1.1;
    const double factor = (rel.y() < 0) ? zoom_factor : 1.0 / zoom_factor;

    m_view_window.zoom(factor, mouse_ratio);
    return Canvas::scroll_event(p, rel);
}

void Graph::nudge_start(const double delta) {
    m_view_window.set_view_start(m_view_window.view_start() + static_cast<int>(delta));
}

void Graph::nudge_end(const double delta) {
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

void Graph::render_line_strip(Shader *shader, const std::vector<float> &verts, const float x_offset, const Vector3f &colour) {
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
void Graph::update() {
    // ---- 1. sample at fixed interval ----
    if (m_frame_count % m_sample_interval == 0) {
        if (!m_paused) {
            m_data_source->sample();
        }
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
    if (visible()) {
        m_stats_widget->set_stats(stats());
        m_stats_widget->update_labels();
    }
    m_frame_count++;
}

void Graph::draw_contents() {

    // ---- draw all series ----
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
    const auto first_verts = first_gen.generate_vertices(m_view_window, n_samples);
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

        const auto scroll_offset = static_cast<float>(m_view_window.get_scroll_offset());
        render_line_strip(m_shader, verts, scroll_offset, cfg.color);
    }
}