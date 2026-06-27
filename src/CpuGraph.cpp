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

constexpr size_t MAX_POINTS = 1000; // or whatever your max history length is

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
        out vec4 color;
        void main() {
            color = vec4(0.0, 1.0, 0.0, 1.0);
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

void CpuGraph::draw_contents() {
    using namespace nanogui;

    Timestamp now = std::chrono::system_clock::now();
    CpuSample latest_sample = m_cpu_history.sample(now);

    int num_samples = m_cpu_history.num_samples();
    std::cout << "Num samples: " << num_samples << std::endl;

    if (num_samples < 2) {
        // Need to wait until we have at least 2 samples
        return;
    }

    CpuSample prev_sample = m_cpu_history.prev_sample();

    // Resize graph data to match number of cores (if not already sized)
    if (m_graph_data.size() != latest_sample.samples.size()) {
        m_graph_data.resize(latest_sample.samples.size());
    }

    // TODO ignore sample time for now
    for (int i = 0; i < latest_sample.samples.size(); i++) {
        unsigned long cpu_total_diff = latest_sample.samples[i].total_time - prev_sample.samples[i].total_time;
        unsigned long cpu_idle_diff = latest_sample.samples[i].idle_time - prev_sample.samples[i].idle_time;

        float core_usage = 0.0f;
        if (cpu_total_diff > 0) {
            core_usage = static_cast<float>(cpu_total_diff - cpu_idle_diff) / cpu_total_diff;
        }
        if (i == 0) {
            std::cout << "Core 0 usage: " << core_usage << std::endl;
        }
        float y = 2 * core_usage - 1.0f;
        m_graph_data[i].push_back(y);
    }
    // TODO remove data from m_graph_data when we've reached the max number of samples

    for (int i = 0; i < latest_sample.samples.size(); i++) {
        std::vector<float> data = m_graph_data[i];
        int num_points = static_cast<int>(data.size());
        if (num_points < 2) continue;

        std::vector<float> padded(MAX_POINTS * 2, 0.0f);
        for (int j = 0; j < num_points; j++) {
            padded[j * 2 + 0] = -1.0f + 2.0f * (float(j) / std::max(float(num_points - 1), 1.0f)); // x
            padded[j * 2 + 1] = data[j]; // y
        }
        m_shader->set_buffer("points", VariableType::Float32, { MAX_POINTS, 2 }, padded.data());

        // m_shader->begin() FAILS WITH THE ERROR:
        // Shader::begin(): shader "cpu_graph_shader" has an unbound argument "gl_VertexID"!
        // Caught exception in main loop: "cpu_graph_shader": vertex attribute "points" has an invalid dimension (expected ndim=2, got 1)
        m_shader->begin();
        m_shader->draw_array(Shader::PrimitiveType::LineStrip, 0, num_points, false);
        m_shader->end();
    }
}