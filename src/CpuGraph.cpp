#include "CpuGraph.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <ostream>
#include <sstream>
#include <vector>
#include <nanogui/opengl.h>
#include <nanogui/widget.h>
#include <nanogui/renderpass.h>
#include <nanogui/theme.h>
#include <GLFW/glfw3.h>

using nanogui::Vector3f;
using nanogui::Matrix4f;
using nanogui::Shader;

constexpr float Pi = 3.14159f;

// Number of samples of the underlying data to take when displaying as a graph
static constexpr size_t GRAPH_DATA_MAX_POINTS = 30;

// How many times per second we want to sample
static constexpr int SAMPLE_FREQUENCY = 1;

// Number of samples to average over to calculate the value of each point on the graph
static constexpr int SAMPLE_WINDOW_SIZE = 2;

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

static const Vector3f GRID_COLOUR = {0.0f, 0.4f, 0.0f};

int frame_count = 0;

CpuGraph::CpuGraph(Widget *parent)
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

    // Determine number of cores from the sampler
    CpuTimesSampler sampler;
    auto stat = sampler.sample();
    size_t num_cores = stat.size();

    // Create one LineGraph per core with a value_func lambda that interpolates
    // sample data for the corresponding core_id
    m_line_graphs.reserve(num_cores);
    for (size_t core_id = 0; core_id < num_cores; core_id++) {
        m_line_graphs.emplace_back(
            GRAPH_DATA_MAX_POINTS,
            [this, core_id](double sample_x) -> double {
                // sample_x is the data-space x, which corresponds to a sample index
                const int num_samples = m_cpu_history.num_samples();
                // Clamp to valid range; return 0.0 for out-of-range
                if (sample_x < SAMPLE_WINDOW_SIZE || sample_x > num_samples - 1) {
                    return 0.0;
                }
                const float y = compute_core_y(
                    static_cast<int>(core_id),
                    static_cast<float>(sample_x),
                    SAMPLE_WINDOW_SIZE);
                // compute_core_y returns -1..1, convert to 0..1 for LineGraph
                return (y + 1.0) / 2.0;
            }
        );
        // Initialise the window to put the first elements to the right of the graph
        m_line_graphs.back().set_start_x(0.0-GRAPH_DATA_MAX_POINTS);
        m_line_graphs.back().set_end_x(-2.0);
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

void CpuGraph::draw(NVGcontext *ctx) {
    // Let Canvas do its normal OpenGL rendering (including draw_contents)
    Canvas::draw(ctx);

    // Now overlay grid line labels using NanoVG
    // if (!m_line_graphs.empty()) {
    //     const size_t num_points = m_line_graphs[0].size();
    //     if (num_points > 0) {
    //         nvgFontSize(ctx, 14.0f);
    //         nvgFontFace(ctx, "sans");
    //         nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    //         nvgFillColor(ctx, m_theme->m_text_color);
    //
    //         const double x_offset = m_line_graphs[0].get_x_offset();
    //
    //         for (size_t i = 0; i < num_points; i++) {
    //             const float ndc_x = m_line_graphs[0].data()[i * 2] - static_cast<float>(x_offset);
    //
    //             // Convert NDC x (-1..1) to pixel x within the widget
    //             float px = m_pos.x() + (ndc_x + 1.0f) * 0.5f * m_size.x();
    //
    //             // Pixel y = top of the widget (with small padding)
    //             float py = m_pos.y() + 4.0f;
    //
    //             std::ostringstream oss;
    //             oss << std::fixed << std::setprecision(2) << m_line_graphs[0].get_sample_x(static_cast<int>(i));
    //             nvgText(ctx, px, py, oss.str().c_str(), nullptr);
    //         }
    //     }
    // }
}

bool CpuGraph::mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) {
    if (m_mouse_over_callback) {
        float x = 2.0f * p.x() / static_cast<float>(m_size.x()) - 1.0f;
        float y = 1.0f - 2.0f * p.y() / static_cast<float>(m_size.y());
        m_mouse_over_callback(x, y);
    }
    return Canvas::mouse_motion_event(p, rel, button, modifiers);
}

bool CpuGraph::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
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
            // End dragging — keep drag offset, just restore pause state
            m_dragging = false;
            set_paused(m_was_paused_before_drag);
            return true;
        }
    }
    return Canvas::mouse_button_event(p, button, down, modifiers);
}

bool CpuGraph::mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) {
    if (button == 1 && m_dragging) {
        if (m_size.x() == 0) {
            return true;
        }
        // Delegate to LineGraph — it accumulates and snaps the drag offset
        for (auto & m_line_graph : m_line_graphs) {
            m_line_graph.apply_drag_offset(-rel.x() * 2.0 / static_cast<double>(m_size.x()));
        }

        return true;
    }
    return Canvas::mouse_drag_event(p, rel, button, modifiers);
}

bool CpuGraph::scroll_event(const Vector2i &p, const Vector2f &rel) {
    // calculate the new left and right positions
    for (auto & m_line_graph : m_line_graphs) {
        const double cur_width = m_line_graph.get_data_width();
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
    }
    return Canvas::scroll_event(p, rel);
}

const CoreSample CpuGraph::interpolate_sample_at(const int core_id, const float x) const {
    const int index_a = std::floor(x);
    if (static_cast<float>(index_a) == x) {
        return m_cpu_history.sample_at(core_id, index_a);
    }

    // std::cout << "interpolating for x: " << x << std::endl;
    const CoreSample& sample_a = m_cpu_history.sample_at(core_id, index_a);
    const CoreSample& sample_b = m_cpu_history.sample_at(core_id, index_a + 1);

    const double t = x - index_a;
    const double interpolated_total_time = static_cast<double>(sample_a.total_time) * (1.0f - t) + static_cast<double>(sample_b.total_time) * t;
    const double interpolated_idle_time = static_cast<double>(sample_a.idle_time) * (1.0f - t) + static_cast<double>(sample_b.idle_time) * t;
    return CoreSample(interpolated_total_time, interpolated_idle_time);
}

// Compute the normalized y-value (-1..1) for a single core from consecutive samples
float CpuGraph::compute_core_y(const int core_id, const float sample_x, const int sample_window_size) const {

    if (sample_x - sample_window_size < 0) {
        return 0;
    }
    const CoreSample prev = interpolate_sample_at(core_id, sample_x - sample_window_size);
    const CoreSample curr = interpolate_sample_at(core_id, sample_x);

    const double cpu_total_diff = curr.total_time - prev.total_time;
    const double cpu_idle_diff = curr.idle_time - prev.idle_time;

    if (cpu_idle_diff > cpu_total_diff) {
        std::cout << "cpu idle diff is greater than total diff. Core id: " << core_id << ", idle diff: " << cpu_idle_diff << ", total diff: " << cpu_total_diff << std::endl;
    }

    float core_usage = 0.0f;
    if (cpu_total_diff > 0) {
        core_usage = (cpu_total_diff - cpu_idle_diff) / cpu_total_diff;
    }
    return 2.0f * core_usage - 1.0f;
}

void CpuGraph::draw_grid(size_t num_points, float x_offset) {

    // Build a buffer with 2 vertices per vertical grid line (bottom to top)
    std::vector<float> grid_points;
    grid_points.reserve(num_points * 4);
    for (size_t i = 0; i < num_points; i++) {
        const float x = m_line_graphs[0].data()[i * 2];
        grid_points.push_back(x);
        grid_points.push_back(-1.0f);
        grid_points.push_back(x);
        grid_points.push_back(1.0f);
    }
    m_grid_shader->set_buffer("points", VariableType::Float32, { num_points * 2, 2 }, grid_points.data());
    m_grid_shader->set_uniform("x_offset", static_cast<float>(x_offset));
    m_grid_shader->set_uniform("line_color", GRID_COLOUR);

    m_grid_shader->begin();
    m_grid_shader->draw_array(Shader::PrimitiveType::Line, 0, num_points * 2, false);
    m_grid_shader->end();
}

void CpuGraph::draw_contents() {
    using namespace nanogui;

    // Advance game time
    m_game_time += 1.0 / 60.0; // one frame step

    const int frame_interval_remainder = frame_count % m_sample_interval;
    if (frame_interval_remainder == 0) {
        // TODO Remove timestamp?
        const Timestamp now = std::chrono::system_clock::now();
        std::cout << "cpu sample at: " << now << ", game_time: " << m_game_time << std::endl;
        m_cpu_history.sample(now);
    }

    frame_count++;

    const int num_samples = m_cpu_history.num_samples();
    if (num_samples < SAMPLE_WINDOW_SIZE + 1) {
        return;
    }

    // Advance all LineGraphs with the current game time
    for (auto& lg : m_line_graphs) {
        lg.advance_time(m_game_time);
    }

    const size_t num_points = m_line_graphs[0].size();
    const auto x_offset = static_cast<float>(m_line_graphs[0].get_x_offset());

    // draw_grid(num_points, x_offset);

    // Render each core's line strip
    for (size_t i = 0; i < m_line_graphs.size(); i++) {
        m_shader->set_buffer("points", VariableType::Float32, { num_points, 2 }, m_line_graphs[i].data());
        m_shader->set_uniform("x_offset", x_offset);
        m_shader->set_uniform("line_color", core_colours[i % num_colours]);

        m_shader->begin();
        m_shader->draw_array(Shader::PrimitiveType::LineStrip, 0, num_points, false);
        m_shader->end();
    }
}