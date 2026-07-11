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

// Maximum number of points to keep in the graph (oldest points are trimmed)
static constexpr size_t GRAPH_DATA_MAX_POINTS = 300;

// How many times per second we want to sample
static constexpr int SAMPLE_FREQUENCY = 6;

// Number of samples to average over to calculate the value of each point on the graph
static constexpr int SAMPLE_WINDOW_SIZE = 2;

static const Vector3f GRAPH_COLOUR = {1.0f, 0.0f, 0.0f};

static const Vector3f GRID_COLOUR = {0.0f, 0.4f, 0.0f};

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

    m_graph_data.resize(GRAPH_DATA_MAX_POINTS * 2, 0.0f);

    // Calculate the number of frames we wait between each sample based on the monitor's refresh rate
    m_sample_interval = 60 / SAMPLE_FREQUENCY;

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

float SinGraph::sample_at(const float x) {
    return sin(x);
}

// Compute the normalized y-value (-1..1) for a single core from consecutive samples
float SinGraph::compute_y(const float sample_x) {
    const float sine_y = sample_at(sample_x);
    return sine_y;
}

void SinGraph::draw_contents() {
    using namespace nanogui;

    if (!m_paused) {
        m_frame_count++;
    }

    const int num_points = GRAPH_DATA_MAX_POINTS;
    // data x value we increase for each sample
    const float step_x = 0.344828;

    const int frame_interval_remainder = m_frame_count % m_sample_interval;
    float scroll_progress = static_cast<float>(frame_interval_remainder) / m_sample_interval;

    // distance between points in screen coordinates
    // (scaled by 1 point so that we always draw the last part of the graph at x >= 1.0)
    constexpr float screen_dx = 2.0f / (num_points - 2);

    if (!m_paused && frame_interval_remainder == 0) {
        m_x_offset += step_x;

        // Compute y-values by sampling sin(x) linearly between start_x and end_x
        for (int j = 0; j < num_points; j++) {
            const float t = static_cast<float>(j) / (num_points - 1);
            const float sample_x = m_start_x + m_x_offset + t * (m_end_x - m_start_x);
            const float y = compute_y(sample_x);
            m_graph_data[j * 2 + 1] = y;
        }
    }

    const float smooth_scrolling_x_offset = scroll_progress * screen_dx;

    // Render each core's line strip
    m_shader->set_buffer("points", VariableType::Float32, { static_cast<size_t>(num_points), 2 }, m_graph_data.data());
    m_shader->set_uniform("x_offset", smooth_scrolling_x_offset);
    m_shader->set_uniform("line_color", GRAPH_COLOUR);

    m_shader->begin();
    m_shader->draw_array(Shader::PrimitiveType::LineStrip, 0, num_points, false);
    m_shader->end();
}
