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

SinGraph::SinGraph(Widget *parent)
    : Canvas(parent, 1) {

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

    m_graph_data.resize(GRAPH_DATA_MAX_POINTS * 2, 0.0f);

    initialise_x_values();
}

void SinGraph::initialise_x_values() {
    // Simplified from: dx = dx_orig * x_scale
    constexpr float dx = 2.0f / (GRAPH_DATA_MAX_POINTS - 2.0f);

    const float left_x = -1.0f;
    for (int i = 0; i < GRAPH_DATA_MAX_POINTS; i++) {
        const float x = left_x + i * dx;
        m_graph_data[i * 2] = x;
        std::cout << "i: " << i << " x: " << x << std::endl;
    }
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

bool SinGraph::scroll_event(const Vector2i &p, const Vector2f &rel) {
    // calculate the new left and right positions
    float cur_width = m_end_x - m_start_x;
    constexpr float scroll_factor = 1.1;
    float new_width;
    if (rel.y() < 0) {
        // zoom out so new width is bigger
        new_width = cur_width * scroll_factor;
    } else {
        new_width = cur_width / scroll_factor;
    }
    // Origin around the current cursor position
    float mouse_x_ratio = p.x() / static_cast<float>(m_size.x());
    const float mouse_x = m_start_x + cur_width * mouse_x_ratio;
    m_start_x = mouse_x - mouse_x_ratio * new_width;
    m_end_x = mouse_x + (1.0f - mouse_x_ratio) * new_width;

    std::cout << "new start_x: " << m_start_x << ", end_x: " << m_end_x << std::endl;

    return Canvas::scroll_event(p, rel);
}

double SinGraph::sample_at(const double x) {
    return sin(x);
}

// Compute the normalized y-value (-1..1) for a single core from consecutive samples
double SinGraph::compute_y(const double sample_x) {
    const double sine_y = sample_at(sample_x);
    return sine_y;
}

void SinGraph::draw_grid(const int num_points, const double smooth_scrolling_x_offset) {
    // Build a buffer with 2 vertices per vertical grid line (bottom to top)
    std::vector<float> grid_points;
    grid_points.reserve(num_points * 4);
    for (int i = 0; i < num_points; i++) {
        float x = m_graph_data[i * 2];
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

    // const int num_points = GRAPH_DATA_MAX_POINTS;
    //
    // nvgFontSize(ctx, 14.0f);
    // nvgFontFace(ctx, "sans");
    // nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    // nvgFillColor(ctx, m_theme->m_text_color);

    // distance between points in screen coordinates
    // (scaled by 1 point so that we always draw the last part of the graph at x >= 1.0)
    // constexpr float screen_dx = 2.0f / (num_points - 2);

    // const float scroll_progress = static_cast<float>(m_frame_count % m_sample_interval) / m_sample_interval;
    //
    // for (int i = 0; i < num_points; i++) {
    //     const float t = static_cast<float>(i) / (num_points - 1);
    //     const float sample_x = m_start_x + m_x_offset + t * (m_end_x - m_start_x);
    //
    //     const float smooth_scrolling_x_offset = scroll_progress * screen_dx;
    //     const float position_x = m_graph_data[i * 2] - smooth_scrolling_x_offset;
    //
    //     // Convert NDC x (-1..1) to pixel x within the widget
    //     float px = m_pos.x() + (position_x + 1.0f) * 0.5f * m_size.x();
    //
    //     // Pixel y = top of the widget (with small padding)
    //     float py = m_pos.y() + 4.0f;
    //
    //     std::ostringstream oss;
    //     oss << std::fixed << std::setprecision(2) << sample_x;
    //     nvgText(ctx, px, py, oss.str().c_str(), nullptr);
    // }
    nvgFontSize(ctx, 16.0f);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << m_sample_interval;
    nvgText(ctx, 40, 20, oss.str().c_str(), nullptr);
}

void SinGraph::draw_contents() {
    using namespace nanogui;

    double current_time = glfwGetTime();
    double delta_time = m_paused ? 0.0 : current_time - m_last_frame_time;
    m_last_frame_time = current_time;
    m_game_time += delta_time;

    const int num_points = GRAPH_DATA_MAX_POINTS;

    // Spacing between adjacent nodes in DATA space -- used only for deciding when we need
    // to resample compute_y(), and for advancing m_x_offset (which is a data-space quantity).
    const double dx_data = (m_end_x - m_start_x) / (num_points - 2);

    // Total distance we should have scrolled, in DATA-space units, since the sim began.
    const double total_scroll = m_scroll_speed * m_game_time;

    // Snap down to the last whole node-spacing we've crossed -- this is the offset that
    // determines which x-values we actually need sampled into the buffer right now.
    const double quantized_offset = std::floor(total_scroll / dx_data) * dx_data;

    // Only re-sample compute_y() across the buffer when we've advanced a full data-space
    // node spacing since the last update.
    if (quantized_offset != m_x_offset) {
        std::cout << "total_scroll: " << total_scroll << ", quantized_offset: " << quantized_offset << ", m_x_offset: " << m_x_offset << ", dx: " << dx_data << std::endl;
        m_x_offset = quantized_offset;

        for (int i = 0; i < num_points; i++) {
            const double t = static_cast<double>(i) / (num_points - 2);
            const double x = m_start_x + m_x_offset + t * (m_end_x - m_start_x);
            const double y = compute_y(x);
            m_graph_data[i * 2 + 1] = static_cast<float>(y);
        }
    }

    // Leftover scroll within the current data-space step, in [0, dx_data).
    const double remainder_data = total_scroll - m_x_offset;
    // Convert that remainder into SCREEN-space units before handing it to the shader,
    // since the shader offsets the screen-space x-values stored in the buffer.
    const double smooth_scrolling_x_offset = remainder_data * 2.0 / (m_end_x - m_start_x);

    m_shader->set_buffer("points", VariableType::Float32, { static_cast<size_t>(num_points), 2 }, m_graph_data.data());
    m_shader->set_uniform("x_offset", static_cast<float>(smooth_scrolling_x_offset));
    m_shader->set_uniform("line_color", GRAPH_COLOUR);
    m_shader->begin();
    m_shader->draw_array(Shader::PrimitiveType::LineStrip, 0, num_points, false);
    m_shader->end();

    // For some reason, this grid gets drawn below the graph line even though we initiate it last
    draw_grid(num_points, smooth_scrolling_x_offset);
}
