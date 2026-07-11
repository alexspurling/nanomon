#include "SinGraph.h"

#include <iostream>
#include <ostream>
#include <sstream>
#include <vector>
#include <cmath>
#include <memory>
#include <iomanip>

#include <nanogui/opengl.h>
#include <nanogui/widget.h>
#include <nanogui/renderpass.h>
#include <nanogui/theme.h>
#include <GLFW/glfw3.h>

using nanogui::Vector3f;
using nanogui::Matrix4f;
using nanogui::Shader;

constexpr float Pi = 3.14159f;

// How many times per second we want to sample
static constexpr int SAMPLE_FREQUENCY = 1;

// Number of samples to average over to calculate the value of each point on the graph
static constexpr int SAMPLE_WINDOW_SIZE = 2;

static const Vector3f GRAPH_COLOUR = {1.0f, 0.0f, 0.0f};

static const Vector3f GRID_COLOUR = {0.0f, 0.2f, 0.0f};

static constexpr size_t MAX_POINTS = 200;

double SinGraph::sin(double x) {
    // sin(x) returns -1..1, map to 0..1
    return (std::sin(x) + 1.0) / 2.0;
}

SinGraph::SinGraph(Widget *parent)
    : Canvas(parent, 1), m_line_graph(MAX_POINTS, &SinGraph::sin) {

    using namespace nanogui;
    m_shader = new Shader(
        render_pass(),
        "cpu_graph_shader",
        // Vertex shader
        R"(#version 330
        in vec2 points;
        uniform float x_offset;
        void main() {
            gl_Position = vec4(points.x - x_offset, points.y, 0.0, 1.0);
        })",
        // Fragment shader
        R"(#version 330
        uniform vec3 line_color;
        out vec4 color;
        void main() {
            color = vec4(line_color, 1.0);
        })"
        );

    m_grid_shader = new Shader(
        render_pass(),
        "cpu_graph_grid_shader",
        // Vertex shader
        R"(#version 330
        in vec2 points;
        uniform float x_offset;
        void main() {
            gl_Position = vec4(points.x - x_offset, points.y, 0.0, 1.0);
        })",
        // Fragment shader
        R"(#version 330
        uniform vec3 line_color;
        out vec4 color;
        void main() {
            color = vec4(line_color, 1.0);
        })"
    );

}


void SinGraph::perform_layout(NVGcontext *ctx) {
    if (parent()) {
        set_position({0, 0});
        set_size(parent()->size());
        std::cout << "set to parent size: " << parent()->size() << std::endl;
    }
    Widget::perform_layout(ctx);
}

bool SinGraph::mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) {
    if (m_mouse_over_callback) {
        float x = 2.0f * p.x() / static_cast<float>(m_size.x()) - 1.0f;
        float y = 1.0f - 2.0f * p.y() / static_cast<float>(m_size.y());
        m_mouse_over_callback(x, y);
    }
    return Canvas::mouse_motion_event(p, rel, button, modifiers);
}

bool SinGraph::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    if (button == 0) {
        if (down) {
            // Start dragging
            m_dragging = true;
            m_was_paused_before_drag = m_paused;
            if (!m_paused) {
                set_paused(true);
            }
            return true;
        }
        if (m_dragging) {
            // End dragging — keep m_drag_offset, just restore pause state
            m_dragging = false;
            set_paused(m_was_paused_before_drag);
            return true;
        }
    }
    return Canvas::mouse_button_event(p, button, down, modifiers);
}

bool SinGraph::mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) {
    if (button == 1 && m_dragging) {
        if (m_size.x() == 0) {
            return true;
        }
        // Accumulate screen-space NDC offset
        m_drag_offset += -rel.x() * 2.0 / static_cast<double>(m_size.x());

        // Check if accumulated drag exceeds one grid spacing in data-space
        const int num_points = static_cast<int>(m_line_graph.size());
        const double dx_data = m_line_graph.get_data_width() / (num_points - 2.0);
        const double data_delta = m_drag_offset * m_line_graph.get_data_width() / 2.0;

        if (std::abs(data_delta) > dx_data) {
            // Number of whole grid spacings to shift
            const int num_steps = static_cast<int>(data_delta / dx_data);
            const double shift = num_steps * dx_data;

            m_line_graph.set_start_x(m_line_graph.start_x() + shift);
            m_line_graph.set_end_x(m_line_graph.end_x() + shift);

            // Recompute y-values at the new sample positions for the shifted window
            m_line_graph.recompute_y_values();

            // Reset m_drag_offset to the remainder (less than one grid spacing)
            const double remainder_data_delta = data_delta - shift;
            m_drag_offset = remainder_data_delta * 2.0 / m_line_graph.get_data_width();
        }

        return true;
    }
    return Canvas::mouse_drag_event(p, rel, button, modifiers);
}

bool SinGraph::scroll_event(const Vector2i &p, const Vector2f &rel) {
    // calculate the new left and right positions
    double cur_width = m_line_graph.get_data_width();
    constexpr double scroll_factor = 1.1;
    double new_width;
    if (rel.y() < 0) {
        // zoom out so new width is bigger
        new_width = cur_width * scroll_factor;
    } else {
        new_width = cur_width / scroll_factor;
    }
    // Origin around the current cursor position
    double mouse_x_ratio = static_cast<double>(p.x()) / static_cast<double>(m_size.x());
    const double mouse_x = m_line_graph.start_x() + cur_width * mouse_x_ratio;
    m_line_graph.set_start_x(mouse_x - mouse_x_ratio * new_width);
    m_line_graph.set_end_x(mouse_x + (1.0 - mouse_x_ratio) * new_width);

    std::cout << "new start_x: " << m_line_graph.start_x() << ", end_x: " << m_line_graph.end_x() << std::endl;

    return Canvas::scroll_event(p, rel);
}


void SinGraph::draw_grid(const int num_points, const double smooth_scrolling_x_offset) {
    // Build a buffer with 2 vertices per vertical grid line (bottom to top)
    std::vector<float> grid_points;
    grid_points.reserve(static_cast<size_t>(num_points) * 4);
    for (int i = 0; i < num_points; i++) {
        float x = m_line_graph.data()[i * 2];
        grid_points.push_back(x);
        grid_points.push_back(-1.0f);
        grid_points.push_back(x);
        grid_points.push_back(1.0f);
    }
    m_grid_shader->set_buffer("points", VariableType::Float32, { static_cast<size_t>(num_points * 2), 2 }, grid_points.data());
    m_grid_shader->set_uniform("x_offset", static_cast<float>(smooth_scrolling_x_offset));
    m_grid_shader->set_uniform("line_color", GRID_COLOUR);

    m_grid_shader->begin();
    m_grid_shader->draw_array(Shader::PrimitiveType::Line, 0, num_points * 2, false);
    m_grid_shader->end();
}

void SinGraph::draw(NVGcontext *ctx) {
    // Let Canvas do its normal OpenGL rendering (including draw_contents)
    Canvas::draw(ctx);

    const size_t num_points = m_line_graph.size();

    // Get smooth_scrolling_x_offset from LineGraph
    const double smooth_scrolling_x_offset = m_line_graph.get_smooth_scrolling_x_offset();

    // Add drag offset so text labels scroll with the graph and grid during drag
    const double total_x_offset = smooth_scrolling_x_offset + m_drag_offset;

    nvgFontSize(ctx, 14.0f);
    nvgFontFace(ctx, "sans");
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(ctx, m_theme->m_text_color);

    for (size_t i = 0; i < num_points; i++) {
        // Compute sample_x from LineGraph
        const double sample_x = m_line_graph.get_sample_x(static_cast<int>(i));

        const float position_x = m_line_graph.data()[i * 2] - static_cast<float>(total_x_offset);

        // Convert NDC x (-1..1) to pixel x within the widget
        float px = m_pos.x() + (position_x + 1.0f) * 0.5f * m_size.x();

        // Pixel y = top of the widget (with small padding)
        float py = m_pos.y() + 4.0f;

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << sample_x;
        nvgText(ctx, px, py, oss.str().c_str(), nullptr);
    }
}

void SinGraph::draw_contents() {
    using namespace nanogui;

    double current_time = glfwGetTime();
    double delta_time = m_paused ? 0.0 : current_time - m_last_frame_time;
    m_last_frame_time = current_time;
    m_game_time += delta_time;

    // Delegate data advancement to LineGraph, passing the computed game time
    m_line_graph.advance_time(m_game_time);

    const size_t num_points = m_line_graph.size();
    const double smooth_scrolling_x_offset = m_line_graph.get_smooth_scrolling_x_offset();

    // Add the drag offset so both graph and grid scroll together during drag
    const double total_x_offset = smooth_scrolling_x_offset + m_drag_offset;

    m_shader->set_buffer("points", VariableType::Float32, { num_points, 2 }, m_line_graph.data());
    m_shader->set_uniform("x_offset", static_cast<float>(total_x_offset));
    m_shader->set_uniform("line_color", GRAPH_COLOUR);
    m_shader->begin();
    m_shader->draw_array(Shader::PrimitiveType::LineStrip, 0, static_cast<int>(num_points), false);
    m_shader->end();

    // For some reason, this grid gets drawn below the graph line even though we initiate it last
    draw_grid(static_cast<int>(num_points), total_x_offset);
}
