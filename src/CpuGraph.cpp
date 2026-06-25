#include "CpuGraph.h"

#include <iostream>
#include <ostream>
#include <nanogui/opengl.h>
#include <nanogui/widget.h>
#include <nanogui/renderpass.h>
#include <GLFW/glfw3.h>

using nanogui::Vector3f;
using nanogui::Matrix4f;
using nanogui::Shader;

constexpr float Pi = 3.14159f;

CpuGraph::CpuGraph(Widget *parent)
    : Canvas(parent, 1) {

    using namespace nanogui;
    m_shader = new Shader(
        render_pass(),
        "cpu_graph_shader",
        // Vertex shader
        R"(#version 330
        uniform mat4 mvp;
        in vec3 position;
        in vec3 color;
        out vec4 frag_color;
        void main() {
            frag_color = vec4(color, 1.0);
            gl_Position = mvp * vec4(position, 1.0);
        })",
        // Fragment shader
        R"(#version 330
        out vec4 color;
        in vec4 frag_color;
        void main() {
            color = frag_color;
        })"
    );

    uint32_t indices[3*12] = {
        3, 2, 6, 6, 7, 3,
        4, 5, 1, 1, 0, 4,
        4, 0, 3, 3, 7, 4,
        1, 5, 6, 6, 2, 1,
        0, 1, 2, 2, 3, 0,
        7, 6, 5, 5, 4, 7
    };

    float positions[3*8] = {
        -1.f, 1.f, 1.f, -1.f, -1.f, 1.f,
        1.f, -1.f, 1.f, 1.f, 1.f, 1.f,
        -1.f, 1.f, -1.f, -1.f, -1.f, -1.f,
        1.f, -1.f, -1.f, 1.f, 1.f, -1.f
    };

    float colors[3*8] = {
        0, 1, 1, 0, 0, 1,
        1, 0, 1, 1, 1, 1,
        0, 1, 0, 0, 0, 0,
        1, 0, 0, 1, 1, 0
    };

    m_shader->set_buffer("indices", VariableType::UInt32, {3*12}, indices);
    m_shader->set_buffer("position", VariableType::Float32, {8, 3}, positions);
    m_shader->set_buffer("color", VariableType::Float32, {8, 3}, colors);
}

void CpuGraph::draw_contents() {
    using namespace nanogui;

    Matrix4f view = Matrix4f::look_at(
        Vector3f(0, -2, -10),
        Vector3f(0, 0, 0),
        Vector3f(0, 1, 0)
    );

    Matrix4f model = Matrix4f::rotate(
        Vector3f(0, 1, 0),
        (float) glfwGetTime()
    );

    Matrix4f proj = Matrix4f::perspective(
        float(25 * Pi / 180),
        0.1f,
        20.f,
        1
    );

    Matrix4f mvp = proj * view * model;

    m_shader->set_uniform("mvp", mvp);

    // Draw 12 triangles starting at index 0
    m_shader->begin();
    m_shader->draw_array(Shader::PrimitiveType::Triangle, 0, 12 * 3, true);
    m_shader->end();
}