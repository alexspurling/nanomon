#include "CpuGraph.h"

#include <iostream>
#include <ostream>
#include <vector>
#include <nanogui/opengl.h>
#include <nanogui/widget.h>
#include <nanogui/renderpass.h>
#include <GLFW/glfw3.h>

using nanogui::Vector3f;
using nanogui::Matrix4f;
using nanogui::Shader;

constexpr float Pi = 3.14159f;

// Maximum number of points to keep in the graph (oldest points are trimmed)
static constexpr size_t GRAPH_DATA_MAX_POINTS = 100;

// Update interval — how often to sample CPU data (milliseconds)
static constexpr long long update_interval_ms = 100;

// Colour palette for each core
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

CpuGraph::CpuGraph(Widget *parent)
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

    // std::vector<float> zeros(MAX_POINTS * 2, 0.0f);
    // m_shader->set_buffer("points", VariableType::Float32, { MAX_POINTS, 2 }, zeros.data());
}

void CpuGraph::perform_layout(NVGcontext *ctx) {
    if (parent()) {
        set_position({0, 0});
        set_size(parent()->size());
        std::cout << "set to parent size: " << parent()->size() << std::endl;
    }
    Widget::perform_layout(ctx);
}

bool CpuGraph::mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) {
    if (m_mouse_over_callback) {
        float x = 2.0f * p.x() / static_cast<float>(m_size.x()) - 1.0f;
        float y = 1.0f - 2.0f * p.y() / static_cast<float>(m_size.y());
        m_mouse_over_callback(x, y);
    }
    return Canvas::mouse_motion_event(p, rel, button, modifiers);
}

void CpuGraph::draw_contents() {
    using namespace nanogui;

    Timestamp now = std::chrono::system_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_last_sample_time).count();

    if (elapsed >= update_interval_ms) {
        m_last_sample_time = now;
        m_cpu_history.sample(now);

        int num_samples = m_cpu_history.num_samples();

        if (num_samples >= 2) {
            CpuSample latest_sample = m_cpu_history.get_latest_sample();
            CpuSample prev_sample = m_cpu_history.prev_sample();

            // Resize graph data to match number of cores (if not already sized)
            if (m_graph_data.size() != latest_sample.samples.size()) {
                m_graph_data.resize(latest_sample.samples.size());
            }

            for (int i = 0; i < static_cast<int>(latest_sample.samples.size()); i++) {
                unsigned long cpu_total_diff = latest_sample.samples[i].total_time -
                    prev_sample.samples[i].total_time;
                unsigned long cpu_idle_diff = latest_sample.samples[i].idle_time -
                    prev_sample.samples[i].idle_time;

                float core_usage = 0.0f;
                if (cpu_total_diff > 0) {
                    core_usage = static_cast<float>(cpu_total_diff - cpu_idle_diff) /
                        static_cast<float>(cpu_total_diff);
                }
                float y = 2 * core_usage - 1.0f;

                // Append new point (x placeholder, y value)
                m_graph_data[i].push_back(0.0f);
                m_graph_data[i].push_back(y);

                // Trim oldest points if we exceed the maximum
                size_t n = m_graph_data[i].size() / 2;
                if (n > GRAPH_DATA_MAX_POINTS) {
                    size_t excess = n - GRAPH_DATA_MAX_POINTS;
                    m_graph_data[i].erase(m_graph_data[i].begin(),
                                          m_graph_data[i].begin() + static_cast<long>(excess * 2));
                }
            }
        }
    }

    if (m_cpu_history.num_samples() < 2) {
        // Need at least 2 samples to render anything
        return;
    }

    // Compute smooth scroll offset based on time since last sample
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_last_sample_time).count();
    float scroll_progress = std::min(
        static_cast<float>(elapsed_ms) / static_cast<float>(update_interval_ms), 1.0f);
    // The number of points shown on the screen is actually n - 2 because we need a buffer at the left and right boundaries
    float dx = 2.0f / static_cast<float>(GRAPH_DATA_MAX_POINTS - 2);
    float scroll_offset = scroll_progress * dx;

    // Update x-values for all cores every frame (smooth scrolling)
    for (int i = 0; i < static_cast<int>(m_graph_data.size()); i++) {
        size_t n = m_graph_data[i].size() / 2;
        if (n < 2) continue;

        for (size_t j = 0; j < n; j++) {
            // By adding dx here we will draw the last point off the screen at x=1, which ensures smooth animation
            float x = 1.0f + dx - static_cast<float>(n - 1 - j) * dx - scroll_offset;
            m_graph_data[i][j * 2] = x;
        }
    }

    // Render each core's line strip
    for (int i = 0; i < static_cast<int>(m_graph_data.size()); i++) {
        size_t num_points = m_graph_data[i].size() / 2;
        if (num_points < 2) continue;

        m_shader->set_buffer("points", VariableType::Float32, { num_points, 2 }, m_graph_data[i].data());
        m_shader->set_uniform("line_color", core_colours[i % num_colours]);

        m_shader->begin();
        m_shader->draw_array(Shader::PrimitiveType::LineStrip, 0, static_cast<int>(num_points), false);
        m_shader->end();
    }
}