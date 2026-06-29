#include "CpuGraph.h"

#include <algorithm>
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

// How many times per second we want to sample
static constexpr int SAMPLE_FREQUENCY = 10;

// Number of samples to average over to calculate the value of each point on the graph
static constexpr int SAMPLE_WINDOW_SIZE = 3;

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

    // Pre-allocate graph data: determine number of cores from the sampler
    CpuTimesSampler sampler;
    auto stat = sampler.sample();
    size_t num_cores = stat.size();
    m_graph_data.resize(num_cores);
    for (auto& core_data : m_graph_data) {
        core_data.resize(GRAPH_DATA_MAX_POINTS * 2, 0.0f);
    }

    // Calculate the number of frames we wait between each sample based on the monitor's refresh rate
    m_sample_interval = 60 / SAMPLE_FREQUENCY;
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

// Compute the normalized y-value (-1..1) for a single core from consecutive samples
float CpuGraph::compute_core_y(const int core_id, const int sample_index, const int sample_window_size) const {

    if (sample_index - sample_window_size < 0) {
        return 0;
    }
    const CoreSample& prev = m_cpu_history.sample_at(core_id, sample_index - sample_window_size);
    const CoreSample& curr = m_cpu_history.sample_at(core_id, sample_index);

    const unsigned long cpu_total_diff = curr.total_time - prev.total_time;
    const unsigned long cpu_idle_diff = curr.idle_time - prev.idle_time;

    float core_usage = 0.0f;
    if (cpu_total_diff > 0) {
        core_usage = static_cast<float>(cpu_total_diff - cpu_idle_diff) /
            static_cast<float>(cpu_total_diff);
    }
    return 2.0f * core_usage - 1.0f;
}

int frame_count = 0;

void CpuGraph::draw_contents() {
    using namespace nanogui;

    const int frame_interval_remainder = frame_count % m_sample_interval;
    if (frame_interval_remainder == 0) {
        // TODO Remove timestamp?
        const Timestamp now = std::chrono::system_clock::now();
        m_cpu_history.sample(now);
    }

    frame_count++;

    const int num_samples = m_cpu_history.num_samples();
    // Rebuild m_graph_data from scratch using all samples from m_cpu_history
    const int num_points = std::min(num_samples - SAMPLE_WINDOW_SIZE, static_cast<int>(GRAPH_DATA_MAX_POINTS));

    if (num_points > 0) {
        const int first_sample_idx = num_samples - num_points;

        for (int core_id = 0; core_id < static_cast<int>(m_graph_data.size()); core_id++) {
            for (int j = 0; j < num_points; j++) {
                const float y = compute_core_y(core_id, first_sample_idx + j, SAMPLE_WINDOW_SIZE);
                m_graph_data[core_id][j * 2] = 0.0f;      // x placeholder
                m_graph_data[core_id][j * 2 + 1] = y;
            }
        }

        // Compute smooth scroll offset based on time since last sample
        const float scroll_progress = static_cast<float>(frame_interval_remainder) / m_sample_interval;
        // The number of points shown on the screen is actually n - 2 because we need a buffer at the left and right boundaries
        constexpr float dx = 2.0f / static_cast<float>(GRAPH_DATA_MAX_POINTS - 2);
        const float scroll_offset = scroll_progress * dx;

        // Update x-values for all cores every frame (smooth scrolling)
        for (int core_id = 0; core_id < static_cast<int>(m_graph_data.size()); core_id++) {
            for (int i = 0; i < num_points; i++) {
                // By adding dx here we will draw the last point off the screen at x=1, which ensures smooth animation
                const float x = 1.0f + dx - static_cast<float>(num_points - 1 - i) * dx - scroll_offset;
                m_graph_data[core_id][i * 2] = x;
            }
        }

        // Render each core's line strip
        for (int i = 0; i < static_cast<int>(m_graph_data.size()); i++) {
            m_shader->set_buffer("points", VariableType::Float32, { static_cast<size_t>(num_points), 2 }, m_graph_data[i].data());
            m_shader->set_uniform("line_color", core_colours[i % num_colours]);

            m_shader->begin();
            m_shader->draw_array(Shader::PrimitiveType::LineStrip, 0, num_points, false);
            m_shader->end();
        }
    }
}