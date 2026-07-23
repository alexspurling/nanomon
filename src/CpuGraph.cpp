#include "CpuGraph.h"

#include <nanogui/renderpass.h>

using nanogui::Vector3f;

// ---- static colour palette ----

const Vector3f CpuGraph::CORE_COLOURS[8] = {
    {0.0f, 1.0f, 0.0f},  // green
    {1.0f, 0.0f, 0.0f},  // red
    {0.0f, 0.5f, 1.0f},  // light blue
    {1.0f, 1.0f, 0.0f},  // yellow
    {1.0f, 0.0f, 1.0f},  // magenta
    {0.0f, 1.0f, 1.0f},  // cyan
    {1.0f, 0.5f, 0.0f},  // orange
    {0.5f, 0.0f, 1.0f},  // purple
};
const size_t CpuGraph::NUM_COLOURS = std::size(CORE_COLOURS);
const Vector3f CpuGraph::GRID_COLOUR = {0.05f, 0.15f, 0.05f};

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
//  CpuGraph
// ============================================================

CpuGraph::CpuGraph(Widget *parent)
    : Canvas(parent, 1, true) {

    using namespace nanogui;

    m_shader = new Shader(render_pass(),
                          "better_cpu_graph_shader",
                          VERT_SRC, FRAG_SRC);

    m_grid_shader = new Shader(render_pass(),
                               "better_cpu_graph_grid_shader",
                               VERT_SRC, FRAG_SRC);

    // The default depth test function for OpenGL is GL_LESS. This meant that
    // when pixels shared the same depth, only the first one to be drawn would
    // be shown. This would result in elements being drawn last showing under
    // elements drawn first. Setting it to GL_LESSEQUAL seems to fix this.
    render_pass()->set_depth_test(RenderPass::DepthTest::LessEqual, false);

    m_sample_interval = 60 / SAMPLE_FREQUENCY;
}

// ---- layout ----

void CpuGraph::perform_layout(NVGcontext *ctx) {
    // Resize the canvas to fill the parent
    if (parent()) {
        set_position({0, 0});
        set_size(parent()->size());
    }
    Canvas::perform_layout(ctx);
}

void CpuGraph::draw(NVGcontext *ctx) {
    Canvas::draw(ctx);
}

// ---- input ----

bool CpuGraph::mouse_button_event(const Vector2i &p, int button,
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

bool CpuGraph::mouse_drag_event(const Vector2i &p, const Vector2i &rel, const int button, const int modifiers) {
    if (button == 1 && m_dragging && m_size.x() > 0) {
        // Convert pixel delta to NDC delta to sample-index delta
        const double ndc_per_pixel = 2.0 / static_cast<double>(m_size.x());
        const double ndc_delta = -rel.x() * ndc_per_pixel;
        const double idx_delta = ndc_delta * m_view_window.view_width() / 2.0;

        m_view_window.set_num_samples(m_cpu_history.num_samples());
        m_view_window.pan(idx_delta);
    }
    return Canvas::mouse_drag_event(p, rel, button, modifiers);
}

bool CpuGraph::scroll_event(const Vector2i &p, const Vector2f &rel) {
    if (m_size.x() == 0)
        return Canvas::scroll_event(p, rel);

    // Find the sample index under the mouse cursor
    const double mouse_ratio = p.x() / static_cast<double>(m_size.x());
    constexpr double zoom_factor = 1.1;
    const double factor = (rel.y() < 0) ? zoom_factor : 1.0 / zoom_factor;

    m_view_window.set_num_samples(m_cpu_history.num_samples());
    m_view_window.zoom(factor, mouse_ratio);

    return Canvas::scroll_event(p, rel);
}

void CpuGraph::nudge_start(const double delta) {
    m_view_window.set_view_start(m_view_window.view_start() + delta);
}

void CpuGraph::nudge_end(const double delta) {
    m_view_window.set_view_end(m_view_window.view_end() + delta);
}

// ---- helpers ----

void CpuGraph::draw_grid(const std::vector<float> &vertices) {
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

void CpuGraph::render_line_strip(Shader *shader,
                                  const std::vector<float> &verts,
                                  const float x_offset,
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

void CpuGraph::draw_contents() {
    using namespace nanogui;

    // ---- 1. sample at fixed interval ----
    if (m_frame_count % m_sample_interval == 0) {
        const int num_samples = m_cpu_history.sample(std::chrono::system_clock::now());

        m_view_window.set_num_samples(num_samples);

        // ---- auto-scroll (when not paused) ----
        if (!m_paused) {
            m_view_window.add_sample();
        }
    }
    if (!m_paused) {
        m_view_window.update_scroll(static_cast<double>(m_frame_count % m_sample_interval) / m_sample_interval);
    }

    // ---- 2. subclass fills in the graph rendering ----
    draw_graph_content();

    m_frame_count++;
}