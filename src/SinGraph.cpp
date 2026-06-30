#include "SinGraph.h"

#include <iostream>
#include <ostream>
#include <vector>
#include <cmath>

#include <nanogui/opengl.h>
#include <nanogui/widget.h>
#include <nanogui/renderpass.h>
#include <GLFW/glfw3.h>

using nanogui::Vector3f;
using nanogui::Matrix4f;
using nanogui::Shader;

constexpr float Pi = 3.14159f;

// Maximum number of points to keep in the graph (oldest points are trimmed)
static constexpr size_t GRAPH_DATA_MAX_POINTS = 30;

// How many times per second we want to sample
static constexpr int SAMPLE_FREQUENCY = 1;

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
        void main() {
            gl_Position = vec4(points.x, points.y, 0.0, 1.0);
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
        void main() {
            gl_Position = vec4(points.x, points.y, 0.0, 1.0);
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

    m_frame_count = 0;
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

float SinGraph::sample_at(const float x) {
    return sin(x);
}

// Compute the normalized y-value (-1..1) for a single core from consecutive samples
float SinGraph::compute_y(const float sample_x) {
    const float sine_y = sample_at(sample_x);
    return sine_y;
}

void SinGraph::draw_grid(const int num_points) {
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
    m_grid_shader->set_uniform("line_color", GRID_COLOUR);

    m_grid_shader->begin();
    m_grid_shader->draw_array(Shader::PrimitiveType::Line, 0, num_points * 2, false);
    m_grid_shader->end();
}

void SinGraph::draw_contents() {
    using namespace nanogui;

    const int num_points = GRAPH_DATA_MAX_POINTS;

    float scroll_progress = 0.0f;

    const int frame_interval_remainder = m_frame_count % m_sample_interval;
    scroll_progress = static_cast<float>(frame_interval_remainder) / m_sample_interval;

    if (!m_paused) {
        if (frame_interval_remainder == 0) {
            m_x_offset += 1.0f;
        }
        m_frame_count++;
    }

    if (num_points > 0) {
        // Compute y-values from the current x offset
        for (int j = 0; j < num_points; j++) {
            const float y = compute_y(m_x_offset + j);
            m_graph_data[j * 2] = 0.0f;      // x placeholder
            m_graph_data[j * 2 + 1] = y;
        }
        // The number of points shown on the screen is actually n - 2 because we need a buffer at the left and right boundaries
        constexpr float dx = 2.0f / static_cast<float>(GRAPH_DATA_MAX_POINTS - 2);
        const float scroll_offset = scroll_progress * dx;

        // Update x-values for all cores every frame (smooth scrolling)
        for (int i = 0; i < num_points; i++) {
            // By adding dx here we will draw the last point off the screen at x=1, which ensures smooth animation
            const float x = 1.0f + dx - static_cast<float>(num_points - 1 - i) * dx - scroll_offset;
            m_graph_data[i * 2] = x;
        }

        draw_grid(num_points);

        // Render each core's line strip
        m_shader->set_buffer("points", VariableType::Float32, { static_cast<size_t>(num_points), 2 }, m_graph_data.data());
        m_shader->set_uniform("line_color", GRAPH_COLOUR);

        m_shader->begin();
        m_shader->draw_array(Shader::PrimitiveType::LineStrip, 0, num_points, false);
        m_shader->end();
    }
}
